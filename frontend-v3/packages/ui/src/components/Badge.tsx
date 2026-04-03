import type { ComponentProps } from 'react';

export interface BadgeProps extends ComponentProps<'span'> {
  variant?: 'default' | 'success' | 'warning' | 'danger' | 'accent' | 'purple';
}

export function Badge({ variant = 'default', className = '', children, ...props }: BadgeProps) {
  const variants = {
    default: 'bg-white/10 text-white/70',
    success: 'bg-[#34c759]/20 text-[#34c759]',
    warning: 'bg-[#ff9500]/20 text-[#ff9500]',
    danger: 'bg-[#ff3b30]/20 text-[#ff3b30]',
    accent: 'bg-[#0071e3]/20 text-[#0071e3]',
    purple: 'bg-[#af52de]/20 text-[#af52de]',
  };
  return (
    <span
      className={`inline-flex items-center rounded-full px-2 py-0.5 text-xs font-medium ${variants[variant]} ${className}`}
      {...props}
    >
      {children}
    </span>
  );
}
