import {
  app,
  BrowserWindow,
  Menu,
  MenuItemConstructorOptions,
  shell,
} from 'electron';

export function setupMenu(win: BrowserWindow): void {
  const template: MenuItemConstructorOptions[] = [
    // App menu
    {
      label: app.name,
      submenu: [
        { role: 'about', label: `About Zacus Studio` },
        { type: 'separator' },
        { label: 'Preferences\u2026', accelerator: 'Cmd+,', click: () => win.webContents.send('menu:preferences') },
        { type: 'separator' },
        { role: 'services' },
        { type: 'separator' },
        { role: 'hide' },
        { role: 'hideOthers' },
        { role: 'unhide' },
        { type: 'separator' },
        { role: 'quit' },
      ],
    },
    // File menu
    {
      label: 'File',
      submenu: [
        {
          label: 'New Scenario',
          accelerator: 'Cmd+N',
          click: () => win.webContents.send('menu:new-scenario'),
        },
        {
          label: 'Open\u2026',
          accelerator: 'Cmd+O',
          click: () => win.webContents.send('menu:open'),
        },
        {
          label: 'Open Recent',
          role: 'recentDocuments',
          submenu: [
            { role: 'clearRecentDocuments' },
          ],
        },
        { type: 'separator' },
        {
          label: 'Save',
          accelerator: 'Cmd+S',
          click: () => win.webContents.send('menu:save'),
        },
        {
          label: 'Save As\u2026',
          accelerator: 'Cmd+Shift+S',
          click: () => win.webContents.send('menu:save-as'),
        },
        { type: 'separator' },
        {
          label: 'Export to SD Card',
          accelerator: 'Cmd+E',
          click: () => win.webContents.send('menu:export-sd'),
        },
        { type: 'separator' },
        { role: 'close' },
      ],
    },
    // Edit menu
    {
      label: 'Edit',
      submenu: [
        { role: 'undo' },
        { role: 'redo' },
        { type: 'separator' },
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'selectAll' },
      ],
    },
    // View menu
    {
      label: 'View',
      submenu: [
        {
          label: 'Editor',
          accelerator: 'Cmd+1',
          click: () => win.webContents.send('menu:tab', 'editor'),
        },
        {
          label: 'Game Master',
          accelerator: 'Cmd+2',
          click: () => win.webContents.send('menu:tab', 'dashboard'),
        },
        {
          label: 'Simulation',
          accelerator: 'Cmd+3',
          click: () => win.webContents.send('menu:tab', 'simulation'),
        },
        {
          label: 'Dev Tools',
          accelerator: 'Cmd+4',
          click: () => win.webContents.send('menu:tab', 'devtools'),
        },
        { type: 'separator' },
        {
          label: 'Compile Scenario',
          accelerator: 'Cmd+B',
          click: () => win.webContents.send('menu:compile'),
        },
        {
          label: 'Validate Scenario',
          accelerator: 'Cmd+R',
          click: () => win.webContents.send('menu:validate'),
        },
        { type: 'separator' },
        { role: 'togglefullscreen' },
        { role: 'reload' },
        { role: 'toggleDevTools' },
      ],
    },
    // Dev menu
    {
      label: 'Dev',
      submenu: [
        {
          label: 'Scan Devices',
          accelerator: 'Cmd+Shift+D',
          click: () => win.webContents.send('menu:scan-devices'),
        },
        {
          label: 'Open Serial Monitor',
          click: () => win.webContents.send('menu:serial-monitor'),
        },
        {
          label: 'Firmware Manager',
          click: () => win.webContents.send('menu:firmware-manager'),
        },
        { type: 'separator' },
        {
          label: 'Show App Logs',
          click: () => shell.openPath(app.getPath('logs')),
        },
        {
          label: 'Show Firmware Cache',
          click: () => {
            const { homedir } = require('os');
            const { join } = require('path');
            shell.openPath(join(homedir(), '.zacus-studio', 'firmwares'));
          },
        },
      ],
    },
    // Window menu
    {
      label: 'Window',
      submenu: [
        { role: 'minimize' },
        { role: 'zoom' },
        { type: 'separator' },
        { role: 'front' },
        { type: 'separator' },
        { role: 'window' },
      ],
    },
    // Help menu
    {
      role: 'help',
      submenu: [
        {
          label: 'Zacus Documentation',
          click: () => shell.openExternal('https://github.com/electron-rare/le-mystere-professeur-zacus'),
        },
        {
          label: 'Report Issue',
          click: () => shell.openExternal('https://github.com/electron-rare/le-mystere-professeur-zacus/issues'),
        },
      ],
    },
  ];

  const menu = Menu.buildFromTemplate(template);
  Menu.setApplicationMenu(menu);
}
