import type { PropsWithChildren, ReactNode } from 'react'

type SectionHeaderProps = PropsWithChildren<{
  title: string
  subtitle?: string
  actions?: ReactNode
}>

export const SectionHeader = ({ title, subtitle, actions }: SectionHeaderProps) => (
  <header className="flex flex-wrap items-start justify-between gap-3">
    <div className="space-y-1">
      <h2 className="text-2xl font-semibold leading-tight text-[var(--ink-900)]">{title}</h2>
      {subtitle ? <p className="text-sm leading-relaxed text-[var(--ink-500)]">{subtitle}</p> : null}
    </div>
    {actions ? <div className="flex flex-wrap items-center gap-2 lg:justify-end">{actions}</div> : null}
  </header>
)
