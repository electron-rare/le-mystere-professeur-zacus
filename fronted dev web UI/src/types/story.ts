export type ScenarioMeta = {
  id: string
  duration_s?: number
  estimated_duration_s?: number
  description?: string
}

export type StreamMessage = {
  type: string
  data?: Record<string, unknown>
  ts?: string
}
