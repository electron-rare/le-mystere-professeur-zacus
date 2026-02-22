import type { ButtonHTMLAttributes, PropsWithChildren } from 'react'

type ButtonVariant = 'primary' | 'secondary' | 'outline' | 'ghost' | 'danger'

type ButtonProps = PropsWithChildren<
  ButtonHTMLAttributes<HTMLButtonElement> & {
    variant?: ButtonVariant
    fullWidth?: boolean
  }
>

const VARIANT_CLASS: Record<ButtonVariant, string> = {
  primary: 'bg-[var(--ink-700)] text-white hover:bg-[var(--ink-900)]',
  secondary: 'bg-[var(--accent-500)] text-white hover:bg-[var(--accent-700)]',
  outline: 'border border-[var(--ink-500)] text-[var(--ink-700)] hover:bg-white/60',
  ghost: 'text-[var(--ink-700)] hover:bg-white/50',
  danger: 'bg-[var(--accent-700)] text-white hover:bg-[var(--accent-900)]',
}

export const Button = ({ variant = 'outline', fullWidth, className = '', children, ...props }: ButtonProps) => (
  <button
    className={`focus-ring min-h-[42px] rounded-full px-4 text-sm font-semibold transition disabled:cursor-not-allowed disabled:opacity-60 ${
      VARIANT_CLASS[variant]
    } ${fullWidth ? 'w-full' : ''} ${className}`}
    {...props}
  >
    {children}
  </button>
)

