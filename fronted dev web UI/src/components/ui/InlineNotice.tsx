import type { PropsWithChildren } from 'react'

type InlineNoticeTone = 'info' | 'success' | 'warning' | 'error'

type InlineNoticeProps = PropsWithChildren<{
  tone?: InlineNoticeTone
  className?: string
}>

const TONE_CLASS: Record<InlineNoticeTone, string> = {
  info: 'border-[var(--mist-400)] text-[var(--ink-700)]',
  success: 'border-[var(--teal-500)] text-[var(--teal-700)]',
  warning: 'border-[var(--accent-500)] text-[var(--accent-700)]',
  error: 'border-[var(--accent-700)] text-[var(--accent-700)]',
}

export const InlineNotice = ({ tone = 'info', className = '', children }: InlineNoticeProps) => (
  <div className={`rounded-2xl border bg-white/70 px-4 py-3 text-sm ${TONE_CLASS[tone]} ${className}`}>{children}</div>
)

