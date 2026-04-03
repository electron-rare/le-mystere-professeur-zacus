import type { ComponentProps } from 'react';

export interface ButtonProps extends ComponentProps<'button'> {
  variant?: 'primary' | 'secondary' | 'ghost' | 'danger';
  size?: 'sm' | 'md' | 'lg';
}

export function Button({ variant = 'primary', size = 'md', className = '', ...props }: ButtonProps) {
  const base =
    'inline-flex items-center justify-center rounded-xl font-medium transition-all focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-[#0071e3] disabled:opacity-50';
  const variants = {
    primary: 'bg-[#0071e3] text-white hover:bg-[#0077ed]',
    secondary: 'bg-[#2c2c2e] text-white hover:bg-[#3a3a3c]',
    ghost: 'bg-transparent text-white hover:bg-white/10',
    danger: 'bg-[#ff3b30] text-white hover:bg-[#ff453a]',
  };
  const sizes = { sm: 'h-8 px-3 text-sm', md: 'h-10 px-4 text-base', lg: 'h-12 px-6 text-lg' };
  return <button className={`${base} ${variants[variant]} ${sizes[size]} ${className}`} {...props} />;
}
