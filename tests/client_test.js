const http = require('http');

const BASE_URL = 'http://localhost:18080';

/**
 * Helper function to make HTTP requests
 */
function request(method, path, body = null) {
    return new Promise((resolve, reject) => {
        const url = `${BASE_URL}${path}`;
        const options = {
            method: method,
            headers: body ? { 'Content-Type': 'application/json' } : {}
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
    console.log('--- Starting MiniDB API Tests ---\n');

    try {
        // 1. Health Check
        console.log('Testing /health...');
        const health = await request('GET', '/health');
        console.log(`Status: ${health.statusCode}, Response: ${health.rawBody}\n`);

        // 2. List Tables
        console.log('Testing /tables...');
        const tables = await request('GET', '/tables');
        console.log(`Status: ${tables.statusCode}, Tables:`, tables.body ? tables.body.tables : 'error', '\n');

        // 3. Create Table
        const testTableName = 'api_test_table';
        console.log(`Testing /create for table: ${testTableName}...`);
        const createData = {
            table_name: testTableName,
            columns: [
                { name: 'id', type: 'INT' },
                { name: 'name', type: 'VARCHAR(50)' },
                { name: 'age', type: 'INT' }
            ]
        };
        const createRes = await request('POST', '/create', createData);
        console.log(`Status: ${createRes.statusCode}, Response: ${createRes.rawBody}\n`);

        // 4. Fetch Metadata
        console.log(`Testing /meta/${testTableName}...`);
        const metaRes = await request('GET', `/meta/${testTableName}`);
        console.log(`Status: ${metaRes.statusCode}`);
        if (metaRes.body) {
            console.log('Metadata:', JSON.stringify(metaRes.body, null, 2));
        }
        console.log();

        // 5. Insert Data
        console.log(`Testing /insert/${testTableName}...`);
        const insertData = {
            id: 101,
            name: 'API Tester',
            age: 25
        };
        const insertRes = await request('POST', `/insert/${testTableName}`, insertData);
        console.log(`Status: ${insertRes.statusCode}, Response: ${insertRes.rawBody}\n`);

        // 6. Fetch Table Data
        console.log(`Testing /table/${testTableName}...`);
        const dataRes = await request('GET', `/table/${testTableName}`);
        console.log(`Status: ${dataRes.statusCode}`);
        if (Array.isArray(dataRes.body)) {
            console.log('Table Contents:');
            console.table(dataRes.body);
        } else {
            console.log('Response:', dataRes.body || dataRes.rawBody);
        }

        console.log('\n--- Tests Completed ---');
    } catch (err) {
        console.error('\nTest execution failed:', err.message);
        console.log('Make sure the MiniDB API server is running at', BASE_URL);
    }
}

runTests();
