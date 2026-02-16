const { defineConfig } = require('cypress');

module.exports = defineConfig({
  e2e: {
    baseUrl: process.env.ESP_UI_URL || 'http://localhost:8080/story-ui',
    specPattern: 'esp32_audio/tests/e2e/**/*.cy.js',
    supportFile: false,
    video: true,
    screenshotOnRunFailure: true,
    defaultCommandTimeout: 10000,
    pageLoadTimeout: 60000,
  },
});
