import { autoUpdater } from 'electron-updater';
import { BrowserWindow } from 'electron';

export function setupAutoUpdater(win: BrowserWindow): void {
  autoUpdater.checkForUpdatesAndNotify();

  autoUpdater.on('update-available', () => {
    win.webContents.send('updater:update-available');
  });

  autoUpdater.on('update-downloaded', () => {
    win.webContents.send('updater:update-downloaded');
  });

  autoUpdater.on('download-progress', (progress: { percent: number }) => {
    win.webContents.send('updater:progress', progress.percent);
  });
}
