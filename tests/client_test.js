const http = require('http');

const BASE_URL = 'http://localhost:18080';

// Command line arguments for authentication
const args = process.argv.slice(2);
if (args.length < 2) {
    console.error('Usage: node tests/client_test.js <username> <password>');
    process.exit(1);
}

const USERNAME = args[0];
const PASSWORD = args[1];

let sessionToken = null;

/**
 * Helper function to make HTTP requests
 */
function request(method, path, body = null, token = null) {
    return new Promise((resolve, reject) => {
        const url = `${BASE_URL}${path}`;
        const headers = {};
        
        if (body) headers['Content-Type'] = 'application/json';
        if (token) headers['X-Session-Token'] = token;

        const options = {
            method: method,
            headers: headers
        };

        const req = http.request(url, options, (res) => {
            let data = '';
            res.on('data', (chunk) => data += chunk);
            res.on('end', () => {
                try {
                    const parsed = data ? JSON.parse(data) : null;
                    resolve({ statusCode: res.statusCode, body: parsed, rawBody: data });
                } catch (e) {
                    resolve({ statusCode: res.statusCode, body: null, rawBody: data });
                }
            });
        });

        req.on('error', (err) => reject(err));
        if (body) req.write(JSON.stringify(body));
        req.end();
    });
}

async function runTests() {
    console.log('--- Starting MiniDB Secure API Tests ---\n');

    try {
        // 1. Login
        console.log(`Logging in as ${USERNAME}...`);
        const loginRes = await request('POST', '/login', { username: USERNAME, password: PASSWORD });
        
        if (loginRes.statusCode !== 200 || !loginRes.body || !loginRes.body.token) {
            console.error(`Login failed (Status ${loginRes.statusCode}):`, loginRes.rawBody);
            return;
        }

        sessionToken = loginRes.body.token;
        console.log('Login successful. Token acquired.\n');

        // 2. Health Check
        console.log('Testing /health...');
        const health = await request('GET', '/health', null, sessionToken);
        console.log(`Status: ${health.statusCode}, Response: ${health.rawBody}\n`);

        // 3. List Tables
        console.log('Testing /tables...');
        const tables = await request('GET', '/tables', null, sessionToken);
        console.log(`Status: ${tables.statusCode}, Tables:`, tables.body ? tables.body.tables : 'error', '\n');

        // 4. Create Table
        const testTableName = 'api_secure_test';
        console.log(`Testing /create for table: ${testTableName}...`);
        const createData = {
            table_name: testTableName,
            columns: [
                { name: 'id', type: 'INT' },
                { name: 'name', type: 'VARCHAR(50)' },
                { name: 'val', type: 'INT' }
            ]
        };
        const createRes = await request('POST', '/create', createData, sessionToken);
        console.log(`Status: ${createRes.statusCode}, Response: ${createRes.rawBody}\n`);

        // 5. Insert Data
        console.log(`Testing /insert/${testTableName}...`);
        const insertData = { id: 500, name: 'Secure Entry', val: 99 };
        const insertRes = await request('POST', `/insert/${testTableName}`, insertData, sessionToken);
        console.log(`Status: ${insertRes.statusCode}, Response: ${insertRes.rawBody}\n`);

        // 6. Bulk Insert Data
        console.log(`Testing /bulk_insert/${testTableName}...`);
        const bulkInsertData = [
            { id: 501, name: 'Bulk Secure 1', val: 10 },
            { id: 502, name: 'Bulk Secure 2', val: 20 }
        ];
        const bulkInsertRes = await request('POST', `/bulk_insert/${testTableName}`, bulkInsertData, sessionToken);
        console.log(`Status: ${bulkInsertRes.statusCode}, Response: ${bulkInsertRes.rawBody}\n`);

        // 7. Fetch Table Data
        console.log(`Testing /table/${testTableName}...`);
        const dataRes = await request('GET', `/table/${testTableName}`, null, sessionToken);
        console.log(`Status: ${dataRes.statusCode}`);
        if (Array.isArray(dataRes.body)) {
            console.table(dataRes.body);
        }

        // 8. Logout
        console.log('\nLogging out...');
        const logoutRes = await request('POST', '/logout', null, sessionToken);
        console.log(`Status: ${logoutRes.statusCode}, Response: ${logoutRes.rawBody}`);

        console.log('\n--- Secure Tests Completed ---');
    } catch (err) {
        console.error('\nTest execution failed:', err.message);
    }
}

runTests();
