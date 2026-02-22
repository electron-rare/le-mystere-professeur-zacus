import type { PropsWithChildren, ReactNode } from 'react'

type FieldProps = PropsWithChildren<{
  label: string
  hint?: ReactNode
  htmlFor?: string
  className?: string
}>

export const Field = ({ label, hint, htmlFor, className = '', children }: FieldProps) => (
  <label className={`block text-xs font-semibold tracking-[0.08em] text-[var(--ink-500)] ${className}`} htmlFor={htmlFor}>
    <span className="uppercase">{label}</span>
    {children}
    {hint ? <span className="mt-1 block text-[11px] font-normal normal-case tracking-normal text-[var(--ink-500)]">{hint}</span> : null}
  </label>
)
