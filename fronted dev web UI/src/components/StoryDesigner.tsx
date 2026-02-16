import { useEffect, useMemo, useState } from 'react'
import CodeMirror from '@uiw/react-codemirror'
import { yaml } from '@codemirror/lang-yaml'

type ValidationResult = {
  valid: boolean
  errors?: string[]
}

type DeployResult = {
  deployed?: string
  status: 'ok' | 'error'
  message?: string
}

type StoryDesignerProps = {
  onValidate: (yaml: string) => Promise<ValidationResult>
  onDeploy: (yaml: string) => Promise<DeployResult>
  onTestRun: (yaml: string) => Promise<void>
}

const TEMPLATE_LIBRARY: Record<string, string> = {
  DEFAULT: '# Template placeholder. Replace with real spec.\nscenario_id: DEFAULT\nsteps: []\n',
  EXPRESS: '# Template placeholder. Replace with real spec.\nscenario_id: EXPRESS\nsteps: []\n',
  EXPRESS_DONE: '# Template placeholder. Replace with real spec.\nscenario_id: EXPRESS_DONE\nsteps: []\n',
  SPECTRE: '# Template placeholder. Replace with real spec.\nscenario_id: SPECTRE\nsteps: []\n',
}

const StoryDesigner = ({ onValidate, onDeploy, onTestRun }: StoryDesignerProps) => {
  const [draft, setDraft] = useState<string>(() => localStorage.getItem('story-draft') ?? '')
  const [status, setStatus] = useState('')
  const [errors, setErrors] = useState<string[]>([])
  const [busy, setBusy] = useState(false)
  const [selectedTemplate, setSelectedTemplate] = useState('')

  useEffect(() => {
    const timer = window.setTimeout(() => {
      localStorage.setItem('story-draft', draft)
    }, 500)

    return () => window.clearTimeout(timer)
  }, [draft])

  useEffect(() => {
    const handleBeforeUnload = (event: BeforeUnloadEvent) => {
      if (draft.trim().length === 0) {
        return
      }
      event.preventDefault()
      event.returnValue = ''
    }

    window.addEventListener('beforeunload', handleBeforeUnload)
    return () => window.removeEventListener('beforeunload', handleBeforeUnload)
  }, [draft])

  const handleTemplateChange = (value: string) => {
    setSelectedTemplate(value)
    if (value && TEMPLATE_LIBRARY[value]) {
      setDraft(TEMPLATE_LIBRARY[value])
      setStatus('Template loaded. Review and update before deploy.')
      setErrors([])
    }
  }

  const handleValidate = async () => {
    setBusy(true)
    setStatus('')
    setErrors([])
    try {
      const result = await onValidate(draft)
      if (result.valid) {
        setStatus('Validation passed ✓')
      } else {
        setStatus('Validation errors found')
        setErrors(result.errors ?? ['Invalid YAML'])
      }
    } catch (err) {
      setStatus(err instanceof Error ? err.message : 'Validation failed')
    } finally {
      setBusy(false)
    }
  }

  const handleDeploy = async () => {
    setBusy(true)
    setStatus('')
    setErrors([])
    try {
      const result = await onDeploy(draft)
      if (result.status === 'ok') {
        setStatus(`Scenario deployed successfully ${result.deployed ? `(${result.deployed})` : ''}.`)
      } else {
        setStatus(result.message ?? 'Deployment failed')
      }
    } catch (err) {
      setStatus(err instanceof Error ? err.message : 'Deployment failed')
    } finally {
      setBusy(false)
    }
  }

  const handleTestRun = async () => {
    setBusy(true)
    setStatus('')
    setErrors([])
    try {
      await onTestRun(draft)
      setStatus('Test run started. Returning to selector after 30 seconds.')
    } catch (err) {
      setStatus(err instanceof Error ? err.message : 'Test run failed')
    } finally {
      setBusy(false)
    }
  }

  const editorExtensions = useMemo(() => [yaml()], [])

  return (
    <section className="space-y-6">
      <div>
        <h2 className="text-2xl font-semibold">Story Designer</h2>
        <p className="text-sm text-[var(--ink-500)]">Draft YAML scenarios and deploy them to the device.</p>
      </div>
      <div className="grid gap-6 lg:grid-cols-[1.1fr_0.9fr]">
        <div className="glass-panel rounded-3xl p-4">
          <CodeMirror
            value={draft}
            height="60vh"
            extensions={editorExtensions}
            onChange={setDraft}
            basicSetup={{ lineNumbers: true, foldGutter: false }}
          />
        </div>
        <div className="glass-panel flex flex-col gap-4 rounded-3xl p-6">
          <div className="space-y-2">
            <label className="text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]" htmlFor="template">
              Load template
            </label>
            <select
              id="template"
              value={selectedTemplate}
              onChange={(event) => handleTemplateChange(event.target.value)}
              className="focus-ring min-h-[44px] rounded-xl border border-[var(--ink-500)] bg-white/70 px-3 text-sm"
            >
              <option value="">Select a template</option>
              {Object.keys(TEMPLATE_LIBRARY).map((key) => (
                <option key={key} value={key}>
                  {key}
                </option>
              ))}
            </select>
            <p className="text-xs text-[var(--ink-500)]">
              Templates are placeholders. Replace them with the real specs before deploy.
            </p>
          </div>

          <div className="grid gap-3 sm:grid-cols-2">
            <button
              type="button"
              onClick={handleValidate}
              disabled={busy}
              className="focus-ring min-h-[44px] rounded-full border border-[var(--ink-700)] px-4 text-sm font-semibold text-[var(--ink-700)] disabled:opacity-70"
            >
              Validate
            </button>
            <button
              type="button"
              onClick={handleDeploy}
              disabled={busy}
              className="focus-ring min-h-[44px] rounded-full bg-[var(--accent-500)] px-4 text-sm font-semibold text-white disabled:opacity-70"
            >
              Deploy
            </button>
            <button
              type="button"
              onClick={handleTestRun}
              disabled={busy}
              className="focus-ring min-h-[44px] rounded-full border border-[var(--ink-500)] px-4 text-sm font-semibold text-[var(--ink-500)] disabled:opacity-70 sm:col-span-2"
            >
              Test Run (30 sec)
            </button>
          </div>

          {status && (
            <div className="rounded-2xl border border-white/60 bg-white/70 p-4 text-sm text-[var(--ink-700)]">
              {status}
            </div>
          )}

          {errors.length > 0 && (
            <div className="rounded-2xl border border-[var(--accent-700)] bg-white/70 p-4 text-xs text-[var(--accent-700)]">
              {errors.map((error) => (
                <p key={error}>{error}</p>
              ))}
            </div>
          )}
        </div>
      </div>
    </section>
  )
}

export default StoryDesigner
