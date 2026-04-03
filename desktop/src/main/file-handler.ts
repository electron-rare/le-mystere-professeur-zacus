import { dialog, app, IpcMain, BrowserWindow, Notification } from 'electron';
import { writeFile } from 'fs/promises';

export function setupFileHandlers(ipcMain: IpcMain, win: BrowserWindow): void {
  ipcMain.handle('file:open', async (_e, { filters } = {}) => {
    const result = await dialog.showOpenDialog(win, {
      properties: ['openFile'],
      filters: filters ?? [
        { name: 'Zacus Scenarios', extensions: ['yaml', 'yml', 'zacus'] },
        { name: 'All Files', extensions: ['*'] },
      ],
    });

    if (result.canceled || result.filePaths.length === 0) return null;

    const filePath = result.filePaths[0];
    app.addRecentDocument(filePath);
    return filePath;
  });

  ipcMain.handle('file:save', async (_e, { data, defaultPath }: { data: string; defaultPath?: string }) => {
    const result = await dialog.showSaveDialog(win, {
      defaultPath: defaultPath ?? 'scenario.yaml',
      filters: [
        { name: 'Zacus Scenarios', extensions: ['yaml', 'yml'] },
        { name: 'Zacus Binary', extensions: ['zacus'] },
      ],
    });

    if (result.canceled || !result.filePath) return null;

    await writeFile(result.filePath, data, 'utf-8');
    app.addRecentDocument(result.filePath);
    return result.filePath;
  });

  ipcMain.handle('file:recent', async () => {
    // macOS manages recent documents via NSDocumentController
    // Return empty array; the menu:open-recent event handles it natively
    return [];
  });

  ipcMain.handle('file:add-recent', async (_e, filePath: string) => {
    app.addRecentDocument(filePath);
  });

  ipcMain.handle('notify', async (_e, { title, body }: { title: string; body: string }) => {
    if (Notification.isSupported()) {
      new Notification({ title, body }).show();
    }
  });
}
