import type { PropsWithChildren } from 'react'

type BadgeTone = 'neutral' | 'info' | 'success' | 'warning' | 'error'

type BadgeProps = PropsWithChildren<{
  tone?: BadgeTone
  className?: string
}>

const TONE_CLASS: Record<BadgeTone, string> = {
  neutral: 'border-[var(--mist-500)] bg-white/75 text-[var(--ink-700)]',
  info: 'border-[var(--ink-500)] bg-[var(--ink-soft)] text-[var(--ink-700)]',
  success: 'border-[var(--teal-500)] bg-[var(--teal-soft)] text-[var(--teal-800)]',
  warning: 'border-[var(--accent-500)] bg-[var(--accent-soft)] text-[var(--accent-800)]',
  error: 'border-[var(--danger-700)] bg-[var(--danger-soft)] text-[var(--danger-800)]',
}

export const Badge = ({ tone = 'neutral', className = '', children }: BadgeProps) => (
  <span
    className={`inline-flex items-center rounded-full border px-2.5 py-1 text-[10px] font-semibold uppercase tracking-[0.14em] ${TONE_CLASS[tone]} ${className}`}
  >
    {children}
  </span>
)
