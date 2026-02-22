import type { PropsWithChildren } from 'react'

type InlineNoticeTone = 'info' | 'success' | 'warning' | 'error'

type InlineNoticeProps = PropsWithChildren<{
  tone?: InlineNoticeTone
  className?: string
}>

const TONE_CLASS: Record<InlineNoticeTone, string> = {
  info: 'border-[var(--mist-500)] bg-[var(--ink-soft)] text-[var(--ink-700)]',
  success: 'border-[var(--teal-500)] bg-[var(--teal-soft)] text-[var(--teal-800)]',
  warning: 'border-[var(--accent-500)] bg-[var(--accent-soft)] text-[var(--accent-800)]',
  error: 'border-[var(--danger-700)] bg-[var(--danger-soft)] text-[var(--danger-800)]',
}

export const InlineNotice = ({ tone = 'info', className = '', children }: InlineNoticeProps) => (
  <div className={`rounded-2xl border px-4 py-3 text-sm leading-relaxed ${TONE_CLASS[tone]} ${className}`}>{children}</div>
)
