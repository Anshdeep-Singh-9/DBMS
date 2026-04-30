import { useState, useEffect } from 'react'
import axios from 'axios'
import { 
  Database, 
  Plus, 
  RefreshCw, 
  Table as TableIcon, 
  LayoutDashboard, 
  LogOut, 
  Trash2, 
  Search,
  CheckCircle2,
  AlertCircle,
  Loader2
} from 'lucide-react'

// Types
interface TableMeta {
  table_name: string;
  column_count: number;
  record_size: number;
  columns: {
    name: string;
    type: string;
    size: number;
  }[];
}

interface TableList {
  tables: string[];
}

function App() {
  const [token, setToken] = useState<string | null>(localStorage.getItem('token'))
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [tables, setTables] = useState<string[]>([])
  const [selectedTable, setSelectedTable] = useState<string | null>(null)
  const [tableMeta, setTableMeta] = useState<TableMeta | null>(null)
  const [tableData, setTableData] = useState<any[]>([])
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [success, setSuccess] = useState<string | null>(null)
  const [isRegistering, setIsRegistering] = useState(false)
  const [apiStatus, setApiStatus] = useState<'online' | 'offline' | 'checking'>('checking')

  // Create Table State
  const [showCreateModal, setShowCreateModal] = useState(false)
  const [newTableName, setNewTableName] = useState('')
  const [newColumns, setNewColumns] = useState([{ name: '', type: 'INT' }])

  // Insert Row State
  const [showInsertForm, setShowInsertForm] = useState(false)
  const [insertFormData, setInsertFormData] = useState<Record<string, any>>({})

  // Raw Query State
  const [rawQuery, setRawQuery] = useState('')

  const api = axios.create({
    baseURL: '/api',
    headers: {
      'X-Session-Token': token || ''
    }
  })

  useEffect(() => {
    checkHealth()
    if (token) {
      fetchTables()
    }
  }, [token])

  const checkHealth = async () => {
    try {
      await axios.get('/api/health')
      setApiStatus('online')
    } catch (err) {
      setApiStatus('offline')
    }
  }

  const fetchTables = async () => {
    try {
      setLoading(true)
      const res = await api.get<TableList>('/tables')
      setTables(res.data.tables)
      setError(null)
    } catch (err: any) {
      setError('Failed to fetch tables. Are you logged in?')
      if (err.response?.status === 401) handleLogout()
    } finally {
      setLoading(false)
    }
  }

  const handleLogin = async (e: React.FormEvent) => {
    e.preventDefault()
    setLoading(true)
    setError(null)
    try {
      if (isRegistering) {
        await axios.post('/api/register', { username, password })
        setSuccess('Registered successfully! You can now login.')
        setIsRegistering(false)
        return
      }

      const res = await axios.post('/api/login', { username, password })
      const newToken = res.data.token
      setToken(newToken)
      localStorage.setItem('token', newToken)
      setSuccess('Logged in successfully!')
      setTimeout(() => setSuccess(null), 3000)
    } catch (err: any) {
      setError(err.response?.data || (isRegistering ? 'Registration failed' : 'Login failed'))
    } finally {
      setLoading(false)
    }
  }

  const handleLogout = async () => {
    try {
      await api.post('/logout')
    } catch (err) {}
    setToken(null)
    localStorage.removeItem('token')
    setTables([])
    setSelectedTable(null)
  }

  const fetchTableDetails = async (name: string) => {
    setSelectedTable(name)
    setLoading(true)
    setError(null)
    try {
      const [metaRes, dataRes] = await Promise.all([
        api.get<TableMeta>(`/meta/${name}`),
        api.get(`/table/${name}`)
      ])
      setTableMeta(metaRes.data)
      setTableData(Array.isArray(dataRes.data) ? dataRes.data : [])
    } catch (err: any) {
      setError('Failed to load table details')
    } finally {
      setLoading(false)
    }
  }

  const handleRawQuery = async (e: React.FormEvent) => {
    e.preventDefault()
    if (!rawQuery.trim()) return
    setLoading(true)
    setError(null)
    setSuccess(null)
    try {
      const res = await api.post('/query', { query: rawQuery })
      setSuccess(res.data)
      setRawQuery('')
      // Refresh tables list in case it was a CREATE/DROP
      fetchTables()
      // If we are viewing a table, refresh it too
      if (selectedTable) fetchTableDetails(selectedTable)
      setTimeout(() => setSuccess(null), 3000)
    } catch (err: any) {
      setError(err.response?.data || 'Query execution failed')
    } finally {
      setLoading(false)
    }
  }

  const handleCreateTable = async (e: React.FormEvent) => {
    e.preventDefault()
    setLoading(true)
    setError(null)
    try {
      await api.post('/create', {
        table_name: newTableName,
        columns: newColumns.filter(c => c.name.trim() !== '')
      })
      setSuccess(`Table ${newTableName} created successfully!`)
      setShowCreateModal(false)
      setNewTableName('')
      setNewColumns([{ name: '', type: 'INT' }])
      fetchTables()
      setTimeout(() => setSuccess(null), 3000)
    } catch (err: any) {
      setError(err.response?.data || 'Failed to create table')
    } finally {
      setLoading(false)
    }
  }

  const handleInsertRow = async (e: React.FormEvent) => {
    e.preventDefault()
    if (!selectedTable) return
    setLoading(true)
    setError(null)
    try {
      // Convert types based on meta
      const processedData: Record<string, any> = {}
      tableMeta?.columns.forEach(col => {
        const val = insertFormData[col.name]
        processedData[col.name] = col.type === 'INT' ? parseInt(val) : val
      })

      await api.post(`/insert/${selectedTable}`, processedData)
      setSuccess('Row inserted successfully!')
      setShowInsertForm(false)
      setInsertFormData({})
      fetchTableDetails(selectedTable)
      setTimeout(() => setSuccess(null), 3000)
    } catch (err: any) {
      setError(err.response?.data || 'Failed to insert row')
    } finally {
      setLoading(false)
    }
  }

  const addColumnField = () => {
    setNewColumns([...newColumns, { name: '', type: 'INT' }])
  }

  const removeColumnField = (index: number) => {
    setNewColumns(newColumns.filter((_, i) => i !== index))
  }

  const updateColumnField = (index: number, field: 'name' | 'type', value: string) => {
    const updated = [...newColumns]
    updated[index][field] = value
    setNewColumns(updated)
  }

  const handleSeed = async () => {
    setLoading(true)
    setError(null)
    setSuccess(null)
    try {
      // 1. Create SeedTable
      setSuccess('Creating SeedTable...')
      await api.post('/create', {
        table_name: 'SeedTable',
        columns: [
          { name: 'id', type: 'INT' },
          { name: 'name', type: 'VARCHAR' },
          { name: 'score', type: 'INT' }
        ]
      })

      // 2. Prepare 1000 lines
      setSuccess('Generating 1000 records...')
      const records = []
      for (let i = 1; i <= 1000; i++) {
        records.push({
          id: i,
          name: `User_${Math.floor(Math.random() * 10000)}`,
          score: Math.floor(Math.random() * 100)
        })
      }

      // 3. Bulk Insert
      setSuccess('Inserting 1000 records...')
      await api.post('/bulk_insert/SeedTable', records)

      setSuccess('Successfully seeded 1000 records into SeedTable!')
      fetchTables()
      setTimeout(() => setSuccess(null), 5000)
    } catch (err: any) {
      setError(err.response?.data || 'Seeding failed')
      setSuccess(null)
    } finally {
      setLoading(false)
    }
  }

  if (!token) {
    return (
      <div className="login-container">
        <div className="card login-card">
          <div className="flex items-center gap-2 mb-4 justify-between">
            <h1 style={{ fontSize: '1.5rem', margin: 0 }}>
              {isRegistering ? 'MiniDB Register' : 'MiniDB Login'}
            </h1>
            <Database size={24} color="#646cff" />
          </div>
          
          {success && (
            <div className="flex items-center gap-2 mb-4" style={{ color: '#10b981' }}>
              <CheckCircle2 size={16} />
              <span className="text-sm">{success}</span>
            </div>
          )}

          <form onSubmit={handleLogin}>
            <div className="form-group">
              <label className="text-sm text-gray">Username</label>
              <input 
                type="text" 
                value={username} 
                onChange={e => setUsername(e.target.value)}
                placeholder="Enter username"
                required
              />
            </div>
            <div className="form-group">
              <label className="text-sm text-gray">Password</label>
              <input 
                type="password" 
                value={password} 
                onChange={e => setPassword(e.target.value)}
                placeholder="Enter password"
                required
              />
            </div>
            {error && (
              <div className="flex items-center gap-2 mt-4" style={{ color: '#ef4444' }}>
                <AlertCircle size={16} />
                <span className="text-sm">{error}</span>
              </div>
            )}
            <button type="submit" className="btn-primary mt-4" style={{ width: '100%' }} disabled={loading}>
              {loading ? <Loader2 className="animate-spin" /> : (isRegistering ? 'Register' : 'Login')}
            </button>
            <button 
              type="button" 
              onClick={() => setIsRegistering(!isRegistering)}
              className="mt-4" 
              style={{ width: '100%', backgroundColor: 'transparent', border: 'none', fontSize: '0.875rem' }}
            >
              {isRegistering ? 'Already have an account? Login' : "Don't have an account? Register"}
            </button>
          </form>
        </div>
      </div>
    )
  }

  return (
    <div className="container">
      {/* Header */}
      <header className="flex justify-between items-center mb-4">
        <div className="flex items-center gap-2">
          <Database size={32} color="#646cff" />
          <h1 style={{ margin: 0 }}>MiniDB Dashboard</h1>
          <div className="flex items-center gap-1 ml-4" title={`API is ${apiStatus}`}>
            <div 
              style={{ 
                width: 8, 
                height: 8, 
                borderRadius: '50%', 
                backgroundColor: apiStatus === 'online' ? '#10b981' : apiStatus === 'offline' ? '#ef4444' : '#f59e0b'
              }} 
            />
            <span className="text-sm text-gray" style={{ textTransform: 'capitalize' }}>{apiStatus}</span>
          </div>
        </div>
        <div className="flex gap-2">
          <button onClick={() => setShowCreateModal(true)} className="btn-primary" disabled={loading}>
            <Plus size={18} />
            Create Table
          </button>
          <button onClick={handleSeed} className="btn-success" disabled={loading}>
            <RefreshCw size={18} />
            Seed 1000 Lines
          </button>
          <button onClick={fetchTables} disabled={loading}>
            <RefreshCw size={18} className={loading ? 'animate-spin' : ''} />
            Refresh
          </button>
          <button onClick={handleLogout} style={{ borderColor: '#ef4444', color: '#ef4444' }}>
            <LogOut size={18} />
            Logout
          </button>
        </div>
      </header>

      {/* Notifications */}
      {success && (
        <div className="card mb-4 flex items-center gap-2" style={{ backgroundColor: 'rgba(16, 185, 129, 0.1)', borderColor: '#10b981' }}>
          <CheckCircle2 size={20} color="#10b981" />
          <span style={{ color: '#10b981' }}>{success}</span>
        </div>
      )}
      {error && (
        <div className="card mb-4 flex items-center gap-2" style={{ backgroundColor: 'rgba(239, 68, 68, 0.1)', borderColor: '#ef4444' }}>
          <AlertCircle size={20} color="#ef4444" />
          <span style={{ color: '#ef4444' }}>{error}</span>
        </div>
      )}

      {/* SQL Console */}
      <div className="card mb-4">
        <div className="flex items-center gap-2 mb-4">
          <Search size={20} color="#646cff" />
          <h2 style={{ margin: 0, fontSize: '1.2rem' }}>SQL Console</h2>
        </div>
        <form onSubmit={handleRawQuery} className="flex gap-2">
          <input 
            type="text" 
            value={rawQuery}
            onChange={e => setRawQuery(e.target.value)}
            placeholder="SELECT * FROM Students; or CREATE TABLE ...; or INSERT INTO ...;"
            style={{ flex: 1, fontFamily: 'monospace' }}
          />
          <button type="submit" className="btn-primary" disabled={loading || !rawQuery.trim()}>
            Run Query
          </button>
        </form>
        <p className="text-sm text-gray mt-2">
          Tip: Supported commands are SELECT, CREATE, INSERT, SHOW TABLES, DROP, and UPDATE.
        </p>
      </div>

      <div className="grid">
        {/* Table List */}
        <div className="card">
          <div className="flex items-center gap-2 mb-4">
            <LayoutDashboard size={20} />
            <h2 style={{ margin: 0, fontSize: '1.2rem' }}>Tables</h2>
          </div>
          {tables.length === 0 ? (
            <p className="text-gray text-sm">No tables found. Create one or click Seed.</p>
          ) : (
            <div className="flex flex-direction-column gap-2" style={{ flexDirection: 'column' }}>
              {tables.map(name => (
                <button 
                  key={name}
                  onClick={() => fetchTableDetails(name)}
                  style={{ 
                    justifyContent: 'flex-start',
                    backgroundColor: selectedTable === name ? 'rgba(100, 108, 255, 0.1)' : 'transparent',
                    borderColor: selectedTable === name ? '#646cff' : 'transparent'
                  }}
                >
                  <TableIcon size={16} />
                  {name}
                </button>
              ))}
            </div>
          )}
        </div>

        {/* Table Content */}
        <div className="card" style={{ gridColumn: 'span 2' }}>
          {selectedTable ? (
            <>
              <div className="flex justify-between items-center mb-4">
                <div className="flex items-center gap-2">
                  <TableIcon size={20} />
                  <h2 style={{ margin: 0, fontSize: '1.2rem' }}>{selectedTable}</h2>
                  <span className="badge badge-blue">{tableData.length} Rows</span>
                </div>
                <div className="flex items-center gap-4">
                  <button onClick={() => setShowInsertForm(!showInsertForm)} className="text-sm">
                    {showInsertForm ? 'Cancel' : 'Insert Row'}
                  </button>
                  <div className="text-sm text-gray">
                    {tableMeta?.record_size} bytes per record
                  </div>
                </div>
              </div>

              {showInsertForm && (
                <div className="card mb-4" style={{ backgroundColor: 'rgba(100, 108, 255, 0.05)' }}>
                  <form onSubmit={handleInsertRow} className="grid" style={{ gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: '1rem', alignItems: 'flex-end' }}>
                    {tableMeta?.columns.map(col => (
                      <div key={col.name} className="form-group" style={{ marginBottom: 0 }}>
                        <label className="text-sm text-gray">{col.name} ({col.type})</label>
                        <input 
                          type={col.type === 'INT' ? 'number' : 'text'}
                          value={insertFormData[col.name] || ''}
                          onChange={e => setInsertFormData({...insertFormData, [col.name]: e.target.value})}
                          placeholder={`Enter ${col.name}`}
                          required
                        />
                      </div>
                    ))}
                    <button type="submit" className="btn-primary" disabled={loading}>
                      Save Row
                    </button>
                  </form>
                </div>
              )}

              {loading ? (
                <div className="flex items-center justify-center p-8">
                  <Loader2 className="animate-spin" size={32} />
                </div>
              ) : (
                <div style={{ overflowX: 'auto' }}>
                  <table>
                    <thead>
                      <tr>
                        {tableMeta?.columns.map(col => (
                          <th key={col.name}>
                            {col.name}
                            <div className="text-sm text-gray" style={{ fontWeight: 400 }}>
                              {col.type}({col.size})
                            </div>
                          </th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {tableData.length === 0 ? (
                        <tr>
                          <td colSpan={tableMeta?.columns.length} style={{ textAlign: 'center', padding: '2rem' }} className="text-gray">
                            No records found
                          </td>
                        </tr>
                      ) : (
                        tableData.map((row, i) => (
                          <tr key={i}>
                            {tableMeta?.columns.map(col => (
                              <td key={col.name}>{row[col.name]}</td>
                            ))}
                          </tr>
                        ))
                      )}
                    </tbody>
                  </table>
                </div>
              )}
            </>
          ) : (
            <div className="flex flex-direction-column items-center justify-center p-8 text-gray" style={{ flexDirection: 'column', height: '100%' }}>
              <Search size={48} style={{ marginBottom: '1rem', opacity: 0.5 }} />
              <p>Select a table to view its contents</p>
            </div>
          )}
        </div>
      </div>
    
    {/* Create Table Modal */}
    {showCreateModal && (
      <div style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0,0,0,0.8)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 1000,
        padding: '1rem'
      }}>
        <div className="card" style={{ width: '100%', maxWidth: '600px', maxHeight: '90vh', overflowY: 'auto' }}>
          <div className="flex justify-between items-center mb-4">
            <h2 style={{ margin: 0 }}>Create New Table</h2>
            <button onClick={() => setShowCreateModal(false)} style={{ border: 'none', background: 'none' }}>×</button>
          </div>
          
          <form onSubmit={handleCreateTable}>
            <div className="form-group mb-4">
              <label className="text-sm text-gray">Table Name</label>
              <input 
                type="text" 
                value={newTableName}
                onChange={e => setNewTableName(e.target.value)}
                placeholder="e.g. Users"
                required
              />
            </div>

            <div className="mb-4">
              <div className="flex justify-between items-center mb-2">
                <label className="text-sm text-gray">Columns</label>
                <button type="button" onClick={addColumnField} className="text-sm">
                  <Plus size={14} /> Add Column
                </button>
              </div>
              
              {newColumns.map((col, index) => (
                <div key={index} className="flex gap-2 mb-2">
                  <input 
                    style={{ flex: 2 }}
                    type="text" 
                    placeholder="Column Name"
                    value={col.name}
                    onChange={e => updateColumnField(index, 'name', e.target.value)}
                    required
                  />
                  <select 
                    style={{ 
                      flex: 1, 
                      padding: '0.6em', 
                      borderRadius: '8px', 
                      backgroundColor: '#1a1a1a', 
                      color: 'white',
                      border: '1px solid #333'
                    }}
                    value={col.type}
                    onChange={e => updateColumnField(index, 'type', e.target.value)}
                  >
                    <option value="INT">INT</option>
                    <option value="VARCHAR">VARCHAR</option>
                  </select>
                  {newColumns.length > 1 && (
                    <button type="button" onClick={() => removeColumnField(index)} style={{ color: '#ef4444' }}>
                      <Trash2 size={16} />
                    </button>
                  )}
                </div>
              ))}
              <p className="text-sm text-gray mt-2">Note: The first column must be INT (Primary Key).</p>
            </div>

            <div className="flex gap-2 justify-end mt-6">
              <button type="button" onClick={() => setShowCreateModal(false)}>Cancel</button>
              <button type="submit" className="btn-primary" disabled={loading}>
                Create Table
              </button>
            </div>
          </form>
        </div>
      </div>
    )}

      <style>{`
        .animate-spin {
          animation: spin 1s linear infinite;
        }
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  )
}

export default App
