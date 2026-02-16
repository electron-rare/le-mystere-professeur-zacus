import { useEffect, useRef, useState } from 'react'
import type { StreamMessage } from '../types/story'

type StreamStatus = 'connecting' | 'open' | 'closed' | 'error'

type StreamOptions = {
  onMessage: (message: StreamMessage) => void
}

export const useStoryStream = (url: string, options: StreamOptions) => {
  const { onMessage } = options
  const [status, setStatus] = useState<StreamStatus>('connecting')
  const retryRef = useRef(0)
  const reconnectTimerRef = useRef<number | null>(null)
  const activeRef = useRef(true)
  const socketRef = useRef<WebSocket | null>(null)

  useEffect(() => {
    activeRef.current = true

    const cleanupSocket = () => {
      if (socketRef.current) {
        socketRef.current.onopen = null
        socketRef.current.onclose = null
        socketRef.current.onerror = null
        socketRef.current.onmessage = null
        socketRef.current.close()
        socketRef.current = null
      }
    }

    const scheduleReconnect = () => {
      if (!activeRef.current) {
        return
      }
      const delay = Math.min(1000 * 2 ** retryRef.current, 30000)
      retryRef.current += 1
      reconnectTimerRef.current = window.setTimeout(() => {
        connect()
      }, delay)
    }

    const connect = () => {
      if (!activeRef.current) {
        return
      }

      cleanupSocket()
      setStatus('connecting')
      const socket = new WebSocket(url)
      socketRef.current = socket

      socket.onopen = () => {
        retryRef.current = 0
        setStatus('open')
      }

      socket.onmessage = (event) => {
        try {
          const payload = JSON.parse(event.data) as StreamMessage
          onMessage(payload)
        } catch {
          onMessage({ type: 'error', data: { message: 'Invalid stream payload' } })
        }
      }

      socket.onerror = () => {
        setStatus('error')
      }

      socket.onclose = () => {
        setStatus('closed')
        scheduleReconnect()
      }
    }

    connect()

    return () => {
      activeRef.current = false
      if (reconnectTimerRef.current) {
        window.clearTimeout(reconnectTimerRef.current)
      }
      cleanupSocket()
    }
  }, [url, onMessage])

  return { status }
}
