const http = require('http');

/**
 * Fetches table data from the MiniDB API and displays it.
 * @param {string} tableName - The name of the table to query.
 */
function queryTable(tableName) {
    const url = `http://localhost:18080/table/${tableName}`;

    console.log(`Querying MiniDB for table: ${tableName}...`);

    http.get(url, (res) => {
        let data = '';

        // Check if server returned an error status
        if (res.statusCode !== 200) {
            console.error(`Error: Server returned status code ${res.statusCode}`);
            res.resume();
            return;
        }

        res.on('data', (chunk) => {
            data += chunk;
        });

        res.on('end', () => {
            try {
                const jsonData = JSON.parse(data);

                if (jsonData.error) {
                    console.error(`API Error: ${jsonData.error}`);
                } else if (Array.isArray(jsonData)) {
                    if (jsonData.length === 0) {
                        console.log(`Table "${tableName}" is empty.`);
                    } else {
                        console.log(`\nContents of table "${tableName}":`);
                        console.table(jsonData);
                    }
                } else {
                    console.log('Unexpected response format:', jsonData);
                }
            } catch (e) {
                console.error('Error parsing JSON:', e.message);
                console.log('Raw response:', data);
            }
        });
    }).on('error', (err) => {
        console.error('Connection Error:', err.message);
        console.log('Make sure the MiniDB API server is running (./server)');
    });
}

// Example usage:
// Get table name from command line arguments or default to 'Students'
const tableName = process.argv[2] || 'Students';
queryTable(tableName);
