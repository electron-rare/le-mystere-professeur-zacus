import type { PropsWithChildren } from 'react'

type BadgeTone = 'neutral' | 'info' | 'success' | 'warning' | 'error'

type BadgeProps = PropsWithChildren<{
  tone?: BadgeTone
  className?: string
}>

const TONE_CLASS: Record<BadgeTone, string> = {
  neutral: 'border-[var(--mist-400)] text-[var(--ink-700)]',
  info: 'border-[var(--ink-500)] text-[var(--ink-700)]',
  success: 'border-[var(--teal-500)] text-[var(--teal-700)]',
  warning: 'border-[var(--accent-500)] text-[var(--accent-700)]',
  error: 'border-[var(--accent-700)] text-[var(--accent-700)]',
}

export const Badge = ({ tone = 'neutral', className = '', children }: BadgeProps) => (
  <span
    className={`inline-flex items-center rounded-full border px-2.5 py-1 text-[10px] font-semibold uppercase tracking-[0.15em] ${TONE_CLASS[tone]} ${className}`}
  >
    {children}
  </span>
)

