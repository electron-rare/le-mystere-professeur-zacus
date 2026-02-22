import { useEffect, useRef, useState } from 'react'
import { connectStream } from '../lib/deviceApi'
import type { StreamMessage } from '../types/story'

type StreamStatus = 'connecting' | 'open' | 'closed' | 'error'
type StreamTransport = 'ws' | 'sse' | 'none'
type RecoveryState = 'idle' | 'retrying'

type StreamOptions = {
  onMessage: (message: StreamMessage) => void
}

const MAX_RETRIES = 8

export const useStoryStream = (options: StreamOptions) => {
  const { onMessage } = options
  const [status, setStatus] = useState<StreamStatus>('connecting')
  const [transport, setTransport] = useState<StreamTransport>('none')
  const [retryCount, setRetryCount] = useState(0)
  const [recoveryState, setRecoveryState] = useState<RecoveryState>('idle')

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
      if (!activeRef.current || retryRef.current >= MAX_RETRIES) {
        return
      }

      if (reconnectTimerRef.current) {
        window.clearTimeout(reconnectTimerRef.current)
      }

      const delay = Math.min(1200 * 2 ** retryRef.current, 10000)
      retryRef.current += 1
      setRetryCount(retryRef.current)
      setRecoveryState('retrying')

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
              setRetryCount(0)
              setRecoveryState('idle')
            }

            if (nextStatus === 'closed' || nextStatus === 'error') {
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
            message: error instanceof Error ? error.message : 'Impossible de se connecter au stream story',
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

  return { status, transport, retryCount, recoveryState }
}
