import { setEspNowEnabled, wifiReconnect } from '../deviceApi'
import { runtimeStore } from './runtime.store'

export const networkStore = {
  reconnectWifi: async () => {
    await runtimeStore.runAction('wifiReconnect', wifiReconnect)
    await runtimeStore.refreshSnapshot()
  },

  setEspNow: async (enabled: boolean) => {
    await runtimeStore.runAction(enabled ? 'espNowOn' : 'espNowOff', async () => {
      await setEspNowEnabled(enabled)
    })
    await runtimeStore.refreshSnapshot()
  },
}
