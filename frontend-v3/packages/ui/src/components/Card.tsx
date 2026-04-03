import type { ComponentProps } from 'react';

export interface CardProps extends ComponentProps<'div'> {
  variant?: 'default' | 'elevated' | 'accent';
}

export function Card({ variant = 'default', className = '', children, ...props }: CardProps) {
  const variants = {
    default: 'bg-[#2c2c2e] border border-white/10',
    elevated: 'bg-[#2c2c2e] border border-white/10 shadow-lg',
    accent: 'bg-[#2c2c2e] border border-[#0071e3]/40 shadow-[0_0_20px_rgba(0,113,227,0.2)]',
  };
  return (
    <div className={`rounded-2xl p-4 ${variants[variant]} ${className}`} {...props}>
      {children}
    </div>
  );
}
