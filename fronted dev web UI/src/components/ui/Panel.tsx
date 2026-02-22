import type { HTMLAttributes, PropsWithChildren } from 'react'

type PanelProps = PropsWithChildren<
  HTMLAttributes<HTMLElement> & {
    as?: 'section' | 'article' | 'div' | 'header'
  }
>

export const Panel = ({ as = 'div', className = '', children, ...props }: PanelProps) => {
  const Element = as
  return (
    <Element
      className={`glass-panel rounded-3xl border border-white/70 p-5 shadow-[var(--panel-shadow)] transition duration-200 ${className}`}
      {...props}
    >
      {children}
    </Element>
  )
}
