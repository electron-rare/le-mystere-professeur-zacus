import type { ButtonHTMLAttributes, PropsWithChildren } from 'react'

type ButtonVariant = 'primary' | 'secondary' | 'outline' | 'ghost' | 'danger'
type ButtonSize = 'sm' | 'md' | 'lg'

type ButtonProps = PropsWithChildren<
  ButtonHTMLAttributes<HTMLButtonElement> & {
    variant?: ButtonVariant
    size?: ButtonSize
    fullWidth?: boolean
  }
>

const VARIANT_CLASS: Record<ButtonVariant, string> = {
  primary:
    'border border-transparent bg-[var(--ink-800)] text-white shadow-[0_12px_24px_rgba(31,42,68,0.22)] hover:-translate-y-px hover:bg-[var(--ink-900)]',
  secondary:
    'border border-transparent bg-[var(--accent-500)] text-white shadow-[0_12px_24px_rgba(255,122,89,0.25)] hover:-translate-y-px hover:bg-[var(--accent-700)]',
  outline:
    'border border-[var(--mist-500)] bg-white/70 text-[var(--ink-700)] shadow-[0_4px_12px_rgba(15,23,42,0.08)] hover:-translate-y-px hover:bg-white',
  ghost: 'border border-transparent text-[var(--ink-600)] hover:bg-white/60',
  danger:
    'border border-transparent bg-[var(--danger-700)] text-white shadow-[0_12px_24px_rgba(159,47,47,0.24)] hover:-translate-y-px hover:bg-[var(--danger-800)]',
}

const SIZE_CLASS: Record<ButtonSize, string> = {
  sm: 'min-h-[34px] px-3 text-xs',
  md: 'min-h-[42px] px-4 text-sm',
  lg: 'min-h-[48px] px-5 text-sm',
}

export const Button = ({
  variant = 'outline',
  size = 'md',
  fullWidth,
  className = '',
  children,
  ...props
}: ButtonProps) => (
  <button
    className={`focus-ring inline-flex items-center justify-center rounded-full font-semibold transition duration-150 active:translate-y-0 disabled:cursor-not-allowed disabled:opacity-55 ${
      VARIANT_CLASS[variant]
    } ${SIZE_CLASS[size]} ${fullWidth ? 'w-full' : ''} ${className}`}
    {...props}
  >
    {children}
  </button>
)
