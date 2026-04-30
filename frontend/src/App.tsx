import { useState, useEffect } from 'react'
import axios from 'axios'
import {
  Database, Plus, RefreshCw, Table as TableIcon,
  LogOut, Trash2, Search, CheckCircle2, AlertCircle,
  Loader2, Zap, Terminal, LayoutGrid, ChevronRight
} from 'lucide-react'

interface TableMeta {
  table_name: string; column_count: number; record_size: number;
  columns: { name: string; type: string; size: number }[];
}
interface QueryResult {
  type: 'select' | 'dml' | 'error';
  strategy?: string;
  columns?: { name: string; type: string; size: number }[];
  rows?: Record<string, any>[];
  row_count?: number;
  message?: string;
}

export default function App() {
  const [token, setToken]               = useState<string | null>(localStorage.getItem('token'))
  const [username, setUsername]         = useState('')
  const [password, setPassword]         = useState('')
  const [isRegistering, setIsRegistering] = useState(false)
  const [tables, setTables]             = useState<string[]>([])
  const [selectedTable, setSelectedTable] = useState<string | null>(null)
  const [tableMeta, setTableMeta]       = useState<TableMeta | null>(null)
  const [tableData, setTableData]       = useState<any[]>([])
  const [loading, setLoading]           = useState(false)
  const [error, setError]               = useState<string | null>(null)
  const [success, setSuccess]           = useState<string | null>(null)
  const [apiStatus, setApiStatus]       = useState<'online'|'offline'|'checking'>('checking')
  const [showCreateModal, setShowCreateModal] = useState(false)
  const [newTableName, setNewTableName] = useState('')
  const [newColumns, setNewColumns]     = useState([{ name: '', type: 'INT' }])
  const [showInsertForm, setShowInsertForm] = useState(false)
  const [insertFormData, setInsertFormData] = useState<Record<string, any>>({})
  const [rawQuery, setRawQuery]         = useState('')
  const [queryResult, setQueryResult]   = useState<QueryResult | null>(null)
  const [showQueryResult, setShowQueryResult] = useState(false)

  const api = axios.create({ baseURL: '/api', headers: { 'X-Session-Token': token || '' } })

  useEffect(() => { checkHealth(); if (token) fetchTables() }, [token])

  const checkHealth = async () => {
    try { await axios.get('/api/health'); setApiStatus('online') }
    catch { setApiStatus('offline') }
  }

  const fetchTables = async () => {
    try { setLoading(true); const r = await api.get<{tables:string[]}>('/tables'); setTables(r.data.tables) }
    catch (e: any) { if (e.response?.status === 401) handleLogout() }
    finally { setLoading(false) }
  }

  const handleLogin = async (e: React.FormEvent) => {
    e.preventDefault(); setLoading(true); setError(null)
    try {
      if (isRegistering) {
        await axios.post('/api/register', { username, password })
        setSuccess('Registered! You can now login.'); setIsRegistering(false); return
      }
      const r = await axios.post('/api/login', { username, password })
      setToken(r.data.token); localStorage.setItem('token', r.data.token)
    } catch (e: any) { setError(e.response?.data || 'Authentication failed') }
    finally { setLoading(false) }
  }

  const handleLogout = async () => {
    try { await api.post('/logout') } catch {}
    setToken(null); localStorage.removeItem('token')
    setTables([]); setSelectedTable(null)
  }

  const fetchTableDetails = async (name: string) => {
    setSelectedTable(name); setLoading(true); setShowInsertForm(false)
    try {
      const [m, d] = await Promise.all([api.get<TableMeta>(`/meta/${name}`), api.get(`/table/${name}`)])
      setTableMeta(m.data); setTableData(Array.isArray(d.data) ? d.data : [])
    } catch { setError('Failed to load table') }
    finally { setLoading(false) }
  }

  const handleRawQuery = async (e: React.FormEvent) => {
    e.preventDefault(); if (!rawQuery.trim()) return
    setLoading(true); setError(null); setSuccess(null); setQueryResult(null)
    try {
      const r = await api.post<QueryResult>('/query', { query: rawQuery })
      if (r.data.type === 'select') { setQueryResult(r.data); setShowQueryResult(true); setRawQuery('') }
      else if (r.data.type === 'error') { setError(r.data.message || 'Query error') }
      else {
        setSuccess('Query executed successfully'); setRawQuery('')
        fetchTables(); if (selectedTable) fetchTableDetails(selectedTable)
        setTimeout(() => setSuccess(null), 3000)
      }
    } catch (e: any) { setError(e.response?.data?.message || 'Query failed') }
    finally { setLoading(false) }
  }

  const handleCreateTable = async (e: React.FormEvent) => {
    e.preventDefault(); setLoading(true); setError(null)
    try {
      await api.post('/create', { table_name: newTableName, columns: newColumns.filter(c => c.name.trim()) })
      setSuccess(`Table '${newTableName}' created!`); setShowCreateModal(false)
      setNewTableName(''); setNewColumns([{ name: '', type: 'INT' }]); fetchTables()
      setTimeout(() => setSuccess(null), 3000)
    } catch (e: any) { setError(e.response?.data || 'Failed to create table') }
    finally { setLoading(false) }
  }

  const handleInsertRow = async (e: React.FormEvent) => {
    e.preventDefault(); if (!selectedTable) return
    setLoading(true); setError(null)
    try {
      const processed: Record<string, any> = {}
      tableMeta?.columns.forEach(c => { processed[c.name] = c.type === 'INT' ? parseInt(insertFormData[c.name]) : insertFormData[c.name] })
      await api.post(`/insert/${selectedTable}`, processed)
      setSuccess('Row inserted!'); setShowInsertForm(false); setInsertFormData({})
      fetchTableDetails(selectedTable); setTimeout(() => setSuccess(null), 3000)
    } catch (e: any) { setError(e.response?.data || 'Failed to insert') }
    finally { setLoading(false) }
  }

  const handleDeleteRow = async (pkCol: string, pkVal: any) => {
    if (!selectedTable) return
    if (!window.confirm(`Delete row where ${pkCol} = ${pkVal}?`)) return
    setLoading(true); setError(null)
    try {
      await api.post('/query', { query: `DELETE FROM ${selectedTable} WHERE ${pkCol} = ${pkVal}` })
      setSuccess(`Deleted row where ${pkCol} = ${pkVal}`); fetchTableDetails(selectedTable)
      setTimeout(() => setSuccess(null), 3000)
    } catch (e: any) { setError(e.response?.data?.message || 'Delete failed') }
    finally { setLoading(false) }
  }

  const handleSeed = async () => {
    setLoading(true); setError(null)
    try {
      setSuccess('Creating SeedTable...')
      await api.post('/create', { table_name: 'SeedTable', columns: [{ name: 'id', type: 'INT' }, { name: 'name', type: 'VARCHAR' }, { name: 'score', type: 'INT' }] })
      const records = Array.from({ length: 1000 }, (_, i) => ({ id: i + 1, name: `User_${Math.floor(Math.random() * 10000)}`, score: Math.floor(Math.random() * 100) }))
      setSuccess('Inserting 1000 records...')
      await api.post('/bulk_insert/SeedTable', records)
      setSuccess('Seeded 1000 rows into SeedTable!'); fetchTables()
      setTimeout(() => setSuccess(null), 5000)
    } catch (e: any) { setError(e.response?.data || 'Seed failed') }
    finally { setLoading(false) }
  }

  // ── Login Page ────────────────────────────────────────────
  if (!token) return (
    <div className="login-page">
      <div className="login-card">
        <div className="login-logo">
          <Database size={24} color="#fff" />
        </div>
        <h1 style={{ marginBottom: '0.25rem' }}>{isRegistering ? 'Create Account' : 'Welcome back'}</h1>
        <p className="text-sm text-muted mb-4">{isRegistering ? 'Sign up for MiniDB access' : 'Sign in to MiniDB Dashboard'}</p>

        {success && <div className="notif notif-success mb-3"><CheckCircle2 size={15} />{success}</div>}
        {error   && <div className="notif notif-error   mb-3"><AlertCircle  size={15} />{error}</div>}

        <form onSubmit={handleLogin} style={{ display: 'flex', flexDirection: 'column', gap: '0.75rem' }}>
          <div className="form-group">
            <label className="form-label">Username</label>
            <input value={username} onChange={e => setUsername(e.target.value)} placeholder="Enter username" required />
          </div>
          <div className="form-group">
            <label className="form-label">Password</label>
            <input type="password" value={password} onChange={e => setPassword(e.target.value)} placeholder="Enter password" required />
          </div>
          <button type="submit" className="btn-primary w-full mt-2" disabled={loading} style={{ padding: '0.65rem' }}>
            {loading ? <Loader2 size={16} className="animate-spin" /> : (isRegistering ? 'Create Account' : 'Sign In')}
          </button>
        </form>

        <div className="divider mt-4" />
        <button onClick={() => { setIsRegistering(!isRegistering); setError(null) }}
          style={{ width: '100%', background: 'transparent', border: 'none', color: 'var(--text-muted)', fontSize: '0.8rem' }}>
          {isRegistering ? 'Already have an account? Sign in' : "Don't have an account? Register"}
        </button>
      </div>
    </div>
  )

  // ── Main App ──────────────────────────────────────────────
  return (
    <div className="app-layout">

      {/* Header */}
      <header className="header">
        <div className="flex items-center gap-3">
          <div style={{ background: 'linear-gradient(135deg,#6366f1,#8b5cf6)', borderRadius: 10, padding: '6px', display: 'flex' }}>
            <Database size={18} color="#fff" />
          </div>
          <span style={{ fontWeight: 700, fontSize: '1rem', letterSpacing: '-0.02em' }}>MiniDB</span>
          <span className="badge badge-gray" style={{ marginLeft: '0.25rem' }}>v2.0</span>
          <div className="flex items-center gap-2" style={{ marginLeft: '0.5rem' }}>
            <div className={`status-dot ${apiStatus}`} />
            <span className="text-xs text-muted" style={{ textTransform: 'capitalize' }}>{apiStatus}</span>
          </div>
        </div>

        <div className="flex items-center gap-2">
          <button onClick={() => setShowCreateModal(true)} className="btn-primary" disabled={loading}>
            <Plus size={15} />Create Table
          </button>
          <button onClick={handleSeed} className="btn-success" disabled={loading}>
            <Zap size={15} />Seed 1000
          </button>
          <button onClick={fetchTables} disabled={loading}>
            <RefreshCw size={15} className={loading ? 'animate-spin' : ''} />
          </button>
          <button onClick={handleLogout} className="btn-danger">
            <LogOut size={15} />Logout
          </button>
        </div>
      </header>

      <div className="main-area">

        {/* Sidebar */}
        <aside className="sidebar">
          <div className="sidebar-label">Tables ({tables.length})</div>
          {tables.length === 0 ? (
            <p className="text-xs text-muted" style={{ padding: '0.5rem 0.75rem' }}>No tables yet.</p>
          ) : tables.map(name => (
            <button key={name} className={`sidebar-item ${selectedTable === name ? 'active' : ''}`}
              onClick={() => fetchTableDetails(name)}>
              <TableIcon size={14} style={{ flexShrink: 0 }} />
              <span className="truncate">{name}</span>
              {selectedTable === name && <ChevronRight size={12} style={{ marginLeft: 'auto', flexShrink: 0 }} />}
            </button>
          ))}
        </aside>

        {/* Main Content */}
        <main className="main-content">

          {/* Notifications */}
          {success && <div className="notif notif-success"><CheckCircle2 size={15} />{success}</div>}
          {error   && <div className="notif notif-error"  ><AlertCircle  size={15} />{error}<button onClick={() => setError(null)} style={{ marginLeft: 'auto', background: 'none', border: 'none', color: 'inherit', padding: '0 0.25rem', cursor: 'pointer', fontSize: '1rem' }}>×</button></div>}

          {/* SQL Console */}
          <div className="sql-console">
            <div className="sql-console-header">
              <div className="sql-console-dot" style={{ background: '#ef4444' }} />
              <div className="sql-console-dot" style={{ background: '#f59e0b' }} />
              <div className="sql-console-dot" style={{ background: '#10b981' }} />
              <Terminal size={13} color="var(--text-muted)" style={{ marginLeft: '0.5rem' }} />
              <span style={{ fontSize: '0.75rem', color: 'var(--text-muted)', fontFamily: 'JetBrains Mono, monospace' }}>SQL Console</span>
            </div>
            <form onSubmit={handleRawQuery} className="sql-console-body">
              <span className="sql-prompt">minidb&gt;</span>
              <input className="sql-input" value={rawQuery} onChange={e => setRawQuery(e.target.value)}
                placeholder="SELECT * FROM students WHERE id = 1;" />
              <button type="submit" className="btn-primary" disabled={loading || !rawQuery.trim()} style={{ padding: '0.4rem 0.9rem', flexShrink: 0 }}>
                {loading ? <Loader2 size={14} className="animate-spin" /> : <><Search size={13} />Run</>}
              </button>
            </form>
            <div className="sql-hint">SELECT · INSERT · UPDATE · DELETE · CREATE · DROP · SHOW TABLES</div>
          </div>

          {/* Table View */}
          {selectedTable ? (
            <div className="card table-fill" style={{ padding: 0, overflow: 'hidden' }}>
              {/* Table Header */}
              <div className="flex items-center justify-between" style={{ padding: '1rem 1.25rem', borderBottom: '1px solid var(--border)' }}>
                <div className="flex items-center gap-2">
                  <TableIcon size={16} color="var(--accent-2)" />
                  <h2>{selectedTable}</h2>
                  <span className="badge badge-indigo">{tableData.length} rows</span>
                  {tableMeta && <span className="badge badge-gray">{tableMeta.record_size}B / record</span>}
                </div>
                <div className="flex items-center gap-2">
                  <button onClick={() => fetchTableDetails(selectedTable)} disabled={loading}>
                    <RefreshCw size={13} className={loading ? 'animate-spin' : ''} />
                  </button>
                  <button onClick={() => setShowInsertForm(!showInsertForm)} className={showInsertForm ? '' : 'btn-primary'} style={{ padding: '0.4rem 0.85rem' }}>
                    <Plus size={13} />{showInsertForm ? 'Cancel' : 'Insert Row'}
                  </button>
                </div>
              </div>

              {/* Insert Form */}
              {showInsertForm && (
                <div style={{ padding: '1rem 1.25rem', borderBottom: '1px solid var(--border)', background: 'rgba(99,102,241,0.04)' }}>
                  <form onSubmit={handleInsertRow} style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(180px, 1fr))', gap: '0.75rem', alignItems: 'flex-end' }}>
                    {tableMeta?.columns.map(col => (
                      <div key={col.name} className="form-group" style={{ marginBottom: 0 }}>
                        <label className="form-label">{col.name} <span className="badge badge-gray" style={{ fontSize: '0.6rem' }}>{col.type}</span></label>
                        <input type={col.type === 'INT' ? 'number' : 'text'} value={insertFormData[col.name] || ''}
                          onChange={e => setInsertFormData({ ...insertFormData, [col.name]: e.target.value })}
                          placeholder={`${col.name}...`} required />
                      </div>
                    ))}
                    <button type="submit" className="btn-primary" disabled={loading} style={{ padding: '0.55rem 1rem' }}>
                      {loading ? <Loader2 size={14} className="animate-spin" /> : 'Save Row'}
                    </button>
                  </form>
                </div>
              )}

              {/* Table Data */}
              {loading ? (
                <div className="flex items-center justify-center" style={{ padding: '3rem' }}>
                  <Loader2 size={28} className="animate-spin" style={{ color: 'var(--accent)' }} />
                </div>
              ) : (
                <div className="table-scroll-body">
                  <table className="data-table">
                    <thead>
                      <tr>
                        {tableMeta?.columns.map(col => (
                          <th key={col.name}>
                            {col.name}
                            <span className="badge badge-gray" style={{ marginLeft: '0.4rem', fontSize: '0.6rem', verticalAlign: 'middle' }}>{col.type}</span>
                          </th>
                        ))}
                        <th style={{ width: 50, textAlign: 'center' }}></th>
                      </tr>
                    </thead>
                    <tbody>
                      {tableData.length === 0 ? (
                        <tr><td colSpan={(tableMeta?.columns.length ?? 0) + 1} style={{ textAlign: 'center', padding: '2.5rem', color: 'var(--text-muted)' }}>
                          <LayoutGrid size={28} style={{ margin: '0 auto 0.5rem', opacity: 0.3, display: 'block' }} />
                          No records found
                        </td></tr>
                      ) : tableData.map((row, i) => (
                        <tr key={i}>
                          {tableMeta?.columns.map(col => (
                            <td key={col.name} className={col.type === 'INT' ? 'font-mono' : ''} style={{ fontSize: col.type === 'INT' ? '0.82rem' : '0.875rem' }}>
                              {row[col.name]}
                            </td>
                          ))}
                          <td style={{ textAlign: 'center' }}>
                            <button className="btn-icon-danger" title="Delete row"
                              onClick={() => { const pk = tableMeta?.columns[0]; if (pk) handleDeleteRow(pk.name, row[pk.name]) }}>
                              <Trash2 size={13} />
                            </button>
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              )}
            </div>
          ) : (
            <div className="card flex items-center" style={{ flexDirection: 'column', justifyContent: 'center', padding: '4rem 2rem', gap: '0.75rem', flex: '1 1 0', minHeight: 0 }}>
              <div style={{ width: 56, height: 56, background: 'var(--surface-2)', borderRadius: 16, display: 'flex', alignItems: 'center', justifyContent: 'center', border: '1px solid var(--border-2)' }}>
                <Database size={24} color="var(--text-dim)" />
              </div>
              <p style={{ color: 'var(--text-muted)', fontSize: '0.9rem' }}>Select a table from the sidebar</p>
              <p className="text-xs text-muted">or run a query in the console above</p>
            </div>
          )}
        </main>
      </div>

      {/* ── Create Table Modal ── */}
      {showCreateModal && (
        <div className="modal-overlay" onClick={e => e.target === e.currentTarget && setShowCreateModal(false)}>
          <div className="modal">
            <div className="flex items-center justify-between mb-4">
              <div className="flex items-center gap-2"><Plus size={18} color="var(--accent-2)" /><h2>Create New Table</h2></div>
              <button className="btn-icon" onClick={() => setShowCreateModal(false)} style={{ fontSize: '1.2rem', padding: '0.2rem 0.5rem' }}>×</button>
            </div>

            <form onSubmit={handleCreateTable} style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
              <div className="form-group">
                <label className="form-label">Table Name</label>
                <input value={newTableName} onChange={e => setNewTableName(e.target.value)} placeholder="e.g. Students" required />
              </div>

              <div>
                <div className="flex items-center justify-between mb-2">
                  <label className="form-label">Columns</label>
                  <button type="button" onClick={() => setNewColumns([...newColumns, { name: '', type: 'INT' }])}
                    style={{ fontSize: '0.78rem', padding: '0.25rem 0.6rem' }}>
                    <Plus size={12} />Add Column
                  </button>
                </div>
                {newColumns.map((col, i) => (
                  <div key={i} className="flex gap-2 mb-2">
                    <input style={{ flex: 2 }} placeholder="Column name" value={col.name}
                      onChange={e => { const u = [...newColumns]; u[i].name = e.target.value; setNewColumns(u) }} required />
                    <select style={{ flex: 1 }} value={col.type}
                      onChange={e => { const u = [...newColumns]; u[i].type = e.target.value; setNewColumns(u) }}>
                      <option value="INT">INT</option>
                      <option value="VARCHAR">VARCHAR</option>
                    </select>
                    {newColumns.length > 1 && (
                      <button type="button" className="btn-icon-danger" onClick={() => setNewColumns(newColumns.filter((_, j) => j !== i))}>
                        <Trash2 size={13} />
                      </button>
                    )}
                  </div>
                ))}
                <p className="text-xs text-muted mt-1">⚠ First column must be INT (used as Primary Key)</p>
              </div>

              <div className="flex justify-end gap-2 mt-2">
                <button type="button" onClick={() => setShowCreateModal(false)}>Cancel</button>
                <button type="submit" className="btn-primary" disabled={loading}>
                  {loading ? <Loader2 size={14} className="animate-spin" /> : <><Plus size={14} />Create</>}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* ── Query Result Modal ── */}
      {showQueryResult && queryResult?.type === 'select' && (
        <div className="modal-overlay" onClick={e => e.target === e.currentTarget && setShowQueryResult(false)}>
          <div className="modal modal-wide" style={{ padding: 0, overflow: 'hidden' }}>
            {/* Modal Header */}
            <div className="flex items-center justify-between" style={{ padding: '1rem 1.25rem', borderBottom: '1px solid var(--border)', background: 'rgba(0,0,0,0.2)' }}>
              <div className="flex items-center gap-2">
                <Search size={16} color="var(--accent-2)" />
                <h2>Query Result</h2>
                <span className="badge badge-indigo">{queryResult.row_count} row{queryResult.row_count !== 1 ? 's' : ''}</span>
                {queryResult.strategy && (
                  <span className={`badge ${queryResult.strategy.includes('B+') ? 'badge-green' : 'badge-amber'}`}>
                    <Zap size={10} />{queryResult.strategy}
                  </span>
                )}
              </div>
              <button className="btn-icon" onClick={() => setShowQueryResult(false)} style={{ fontSize: '1.2rem', padding: '0.2rem 0.5rem' }}>×</button>
            </div>

            {/* Result Table */}
            <div style={{ maxHeight: '65vh', overflow: 'auto' }}>
              {queryResult.rows?.length === 0 ? (
                <div className="flex items-center" style={{ flexDirection: 'column', padding: '3rem', gap: '0.75rem', color: 'var(--text-muted)' }}>
                  <Search size={36} style={{ opacity: 0.25 }} /><p>No matching rows found</p>
                </div>
              ) : (
                <table className="data-table">
                  <thead>
                    <tr>
                      {queryResult.columns?.map(col => (
                        <th key={col.name}>{col.name}
                          <span className="badge badge-gray" style={{ marginLeft: '0.35rem', fontSize: '0.6rem', verticalAlign: 'middle' }}>{col.type}</span>
                        </th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {queryResult.rows?.map((row, i) => (
                      <tr key={i}>
                        {queryResult.columns?.map(col => (
                          <td key={col.name} className={col.type === 'INT' ? 'font-mono' : ''} style={{ fontSize: col.type === 'INT' ? '0.82rem' : '0.875rem' }}>
                            {String(row[col.name] ?? '')}
                          </td>
                        ))}
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
            </div>

            <div className="flex justify-end" style={{ padding: '0.75rem 1.25rem', borderTop: '1px solid var(--border)' }}>
              <button className="btn-primary" onClick={() => setShowQueryResult(false)}>Close</button>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
