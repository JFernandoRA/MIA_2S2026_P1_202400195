import { useState, useRef } from 'react'
import './App.css'

const API_URL = 'http://localhost:8080'

const PLACEHOLDER = `mkdisk -size=3000 -unit=K -path=/home/user/Disco1.mia
fdisk -size=300 -path=/home/user/Disco1.mia -name=Particion1
mount -path=/home/user/Disco1.mia -name=Particion1
mkfs -id=951A`

function App() {
  const [input, setInput] = useState('')
  const [output, setOutput] = useState('')
  const [running, setRunning] = useState(false)
  const [connected, setConnected] = useState(null)
  const [lastFileName, setLastFileName] = useState('')
  const fileInputRef = useRef(null)

  async function checkConnection() {
    try {
      const res = await fetch(API_URL + '/')
      setConnected(res.ok)
    } catch {
      setConnected(false)
    }
  }

  async function handleExecute() {
    if (!input.trim()) return
    setRunning(true)
    if (connected === null) await checkConnection()
    try {
      const res = await fetch(API_URL + '/execute', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ commands: input }),
      })
      const data = await res.json()
      setConnected(true)
      setOutput((prev) => (prev ? prev + '\n' : '') + (data.output || data.error || ''))
    } catch (err) {
      setConnected(false)
      setOutput((prev) => (prev ? prev + '\n' : '') + `# no se pudo conectar con ${API_URL} — ¿está corriendo ./server?\n`)
    } finally {
      setRunning(false)
    }
  }

  function handleClear() {
    setInput('')
    setOutput('')
    setLastFileName('')
  }

  function handleFileChoose() {
    fileInputRef.current?.click()
  }

  function handleFileSelected(e) {
    const file = e.target.files[0]
    if (!file) return
    setLastFileName(file.name)
    const reader = new FileReader()
    reader.onload = (ev) => setInput(ev.target.result)
    reader.readAsText(file)
    e.target.value = ''
  }

  function handleKeyDown(e) {
    if (e.key === 'Enter' && (e.metaKey || e.ctrlKey)) {
      e.preventDefault()
      handleExecute()
    }
  }

  return (
    <div className="shell">
      <header className="topbar">
        <div className="brand">
          <DiskMark />
          <span className="brand-name">ExtreamFS</span>
        </div>
        <div className={`status status-${connected === null ? 'unknown' : connected ? 'ok' : 'down'}`}>
          <span className="status-dot" />
          {connected === null ? 'sin verificar' : connected ? 'conectado' : 'sin conexión'}
        </div>
      </header>

      <main className="layout">
        <section className="console">
          <div className="pane">
            <div className="pane-head">
              <span>entrada</span>
              <div className="pane-actions">
                <button className="btn" onClick={handleFileChoose}>elegir archivo .smia</button>
                <input
                  ref={fileInputRef}
                  type="file"
                  accept=".smia,.txt"
                  onChange={handleFileSelected}
                  style={{ display: 'none' }}
                />
                {lastFileName && <span className="filename">{lastFileName}</span>}
              </div>
            </div>
            <textarea
              className="editor"
              value={input}
              onChange={(e) => setInput(e.target.value)}
              onKeyDown={handleKeyDown}
              placeholder={PLACEHOLDER}
              spellCheck={false}
            />
          </div>

          <div className="toolbar">
            <button className="btn btn-primary" onClick={handleExecute} disabled={running}>
              {running ? 'ejecutando…' : 'ejecutar'}
            </button>
            <button className="btn" onClick={handleClear}>limpiar</button>
            <span className="hint">ctrl/cmd + enter para ejecutar</span>
          </div>

          <div className="pane">
            <div className="pane-head">
              <span>salida</span>
            </div>
            <pre className="editor output">{output || '# los resultados de tus comandos aparecerán aquí'}</pre>
          </div>
        </section>

        <aside className="sidebar">
          <div className="sidebar-block">
            <h2>referencia rápida</h2>
            <dl className="cmd-list">
              <dt>mkdisk</dt><dd>-size -unit -path -fit</dd>
              <dt>fdisk</dt><dd>-size -path -name -type -unit -fit</dd>
              <dt>mount</dt><dd>-path -name</dd>
              <dt>mkfs</dt><dd>-id -type</dd>
              <dt>login</dt><dd>-user -pass -id</dd>
              <dt>mkfile</dt><dd>-path -r -size -cont</dd>
              <dt>rep</dt><dd>-name -path -id -path_file_ls</dd>
            </dl>
          </div>
          <div className="sidebar-block">
            <h2>notas</h2>
            <p className="note">El backend corre en <code>localhost:8080</code>. Levántalo con <code>./server</code> desde <code>backend/</code> antes de ejecutar comandos aquí.</p>
          </div>
        </aside>
      </main>
    </div>
  )
}

function DiskMark() {
  return (
    <svg width="22" height="22" viewBox="0 0 22 22" fill="none" aria-hidden="true">
      <circle cx="11" cy="11" r="9.5" stroke="var(--amber)" strokeWidth="1.4" />
      <circle cx="11" cy="11" r="5.5" stroke="var(--amber)" strokeWidth="1.4" />
      <circle cx="11" cy="11" r="1.6" fill="var(--amber)" />
    </svg>
  )
}

export default App