import type { ComponentProps } from 'react';

export interface ProgressProps extends ComponentProps<'div'> {
  value: number;
  max?: number;
  variant?: 'default' | 'success' | 'warning' | 'danger';
  label?: string;
}

export function Progress({
  value,
  max = 100,
  variant = 'default',
  label,
  className = '',
  ...props
}: ProgressProps) {
  const percent = Math.min(100, Math.max(0, (value / max) * 100));
  const variants = {
    default: 'bg-[#0071e3]',
    success: 'bg-[#34c759]',
    warning: 'bg-[#ff9500]',
    danger: 'bg-[#ff3b30]',
  };
  return (
    <div className={`w-full ${className}`} {...props}>
      {label !== undefined && (
        <div className="mb-1 flex justify-between text-xs text-white/60">
          <span>{label}</span>
          <span>{Math.round(percent)}%</span>
        </div>
      )}
      <div className="h-2 w-full overflow-hidden rounded-full bg-white/10">
        <div
          className={`h-full rounded-full transition-all ${variants[variant]}`}
          style={{ width: `${percent}%` }}
          role="progressbar"
          aria-valuenow={value}
          aria-valuemin={0}
          aria-valuemax={max}
        />
      </div>
    </div>
  );
}
