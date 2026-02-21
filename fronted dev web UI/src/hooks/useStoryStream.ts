import { useEffect, useRef, useState } from 'react'
import { connectStream } from '../lib/deviceApi'
import type { StreamMessage } from '../types/story'

type StreamStatus = 'connecting' | 'open' | 'closed' | 'error'
type StreamTransport = 'ws' | 'sse' | 'none'

type StreamOptions = {
  onMessage: (message: StreamMessage) => void
}

export const useStoryStream = (options: StreamOptions) => {
  const { onMessage } = options
  const [status, setStatus] = useState<StreamStatus>('connecting')
  const [transport, setTransport] = useState<StreamTransport>('none')
  const retryRef = useRef(0)
  const reconnectTimerRef = useRef<number | null>(null)
  const activeRef = useRef(true)
  const connectionRef = useRef<{ close: () => void } | null>(null)

  useEffect(() => {
    activeRef.current = true

    const cleanupConnection = () => {
      if (connectionRef.current) {
        connectionRef.current.close()
        connectionRef.current = null
      }
    }

    const scheduleReconnect = () => {
      if (!activeRef.current) {
        return
      }
      if (reconnectTimerRef.current) {
        window.clearTimeout(reconnectTimerRef.current)
      }
      const delay = Math.min(1000 * 2 ** retryRef.current, 10000)
      retryRef.current += 1
      reconnectTimerRef.current = window.setTimeout(() => {
        void connect()
      }, delay)
    }

    const connect = async () => {
      if (!activeRef.current) {
        return
      }

      cleanupConnection()
      setStatus('connecting')
      try {
        const connection = await connectStream({
          onMessage,
          onStatus: (nextStatus) => {
            setStatus(nextStatus)
            if (nextStatus === 'open') {
              retryRef.current = 0
            }
            if (nextStatus === 'closed') {
              scheduleReconnect()
            }
          },
        })
        if (!activeRef.current) {
          connection.close()
          return
        }
        connectionRef.current = connection
        setTransport(connection.kind)
      } catch (error) {
        setStatus('error')
        onMessage({
          type: 'error',
          data: {
            message: error instanceof Error ? error.message : 'Unable to connect to story stream',
          },
        })
        scheduleReconnect()
      }
    }

    void connect()

    return () => {
      activeRef.current = false
      if (reconnectTimerRef.current) {
        window.clearTimeout(reconnectTimerRef.current)
      }
      cleanupConnection()
      setTransport('none')
    }
  }, [onMessage])

  return { status, transport }
}
