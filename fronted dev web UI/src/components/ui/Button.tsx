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
    'border border-transparent bg-gradient-to-b from-[var(--accent-500)] to-[var(--accent-700)] text-white shadow-[0_10px_22px_rgba(255,122,89,0.25)] transition-all hover:-translate-y-0.5 hover:brightness-105 active:translate-y-0 disabled:opacity-70',
  secondary:
    'border border-transparent bg-[var(--ink-700)] text-white shadow-[0_10px_22px_rgba(15,23,42,0.25)] transition-all hover:-translate-y-0.5 hover:brightness-105 active:translate-y-0 disabled:opacity-70',
  outline:
    'border border-[var(--mist-500)] bg-white/72 text-[var(--ink-700)] shadow-[0_4px_12px_rgba(15,23,42,0.08)] backdrop-blur transition-all hover:-translate-y-0.5 hover:bg-white disabled:opacity-70',
  ghost: 'border border-transparent text-[var(--ink-600)] hover:bg-white/60 disabled:opacity-70',
  danger:
    'border border-transparent bg-gradient-to-b from-[var(--danger-700)] to-[var(--danger-800)] text-white shadow-[0_10px_22px_rgba(159,47,47,0.25)] transition-all hover:-translate-y-0.5 active:translate-y-0 disabled:opacity-70',
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
