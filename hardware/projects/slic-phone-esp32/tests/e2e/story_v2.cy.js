const uiUrl = Cypress.env('ESP_UI_URL') || Cypress.config('baseUrl');

const launchScenario = (scenario) => {
  cy.contains(scenario, { timeout: 10000 }).should('be.visible').click();
  cy.contains('Play', { timeout: 10000 }).should('be.visible').click();
  cy.contains('LiveOrchestrator', { timeout: 10000 }).should('be.visible');
};

describe('Story V2 E2E Tests', () => {
  beforeEach(() => {
    cy.visit(uiUrl);
    cy.contains('Scenario Selector', { timeout: 10000 }).should('be.visible');
  });

  it('should select and launch a scenario', () => {
    launchScenario('DEFAULT');
    cy.contains('unlock_event', { timeout: 10000 }).should('be.visible');
  });

  it('should pause and resume execution', () => {
    launchScenario('DEFAULT');

    cy.contains('Pause', { timeout: 10000 }).click();
    cy.contains(/paused/i, { timeout: 4000 }).should('exist');

    cy.contains('Resume', { timeout: 10000 }).click();
    cy.contains(/running/i, { timeout: 4000 }).should('exist');
  });

  it('should skip to next step', () => {
    launchScenario('DEFAULT');

    cy.contains('[Step:', { timeout: 10000 })
      .invoke('text')
      .then((initialStep) => {
        cy.contains('Skip', { timeout: 10000 }).click();
        cy.contains('[Step:', { timeout: 10000 })
          .invoke('text')
          .should((nextStep) => {
            expect(nextStep).not.to.eq(initialStep);
          });
      });
  });

  it('should complete a 4-scenario loop', () => {
    const scenarios = ['DEFAULT', 'EXPRESS', 'EXPRESS_DONE', 'SPECTRE'];

    scenarios.forEach((scenario) => {
      launchScenario(scenario);
      cy.contains(/done/i, { timeout: 300000 }).should('exist');
      cy.contains('Back', { timeout: 10000 }).click();
      cy.contains('Scenario Selector', { timeout: 10000 }).should('be.visible');
    });
  });

  it('should validate YAML in Designer', () => {
    cy.contains('Designer', { timeout: 10000 }).click();
    cy.get('textarea')
      .first()
      .clear()
      .type('invalid: yaml: syntax', { parseSpecialCharSequences: false });
    cy.contains('Validate', { timeout: 10000 }).click();
    cy.contains(/error/i, { timeout: 2000 }).should('exist');
  });

  it('should deploy a scenario', () => {
    cy.contains('Designer', { timeout: 10000 }).click();
    cy.contains('Load template', { timeout: 10000 })
      .parent()
      .find('select')
      .select('EXPRESS');
    cy.contains('Deploy', { timeout: 10000 }).click();
    cy.contains('deployed successfully', { timeout: 5000 }).should('exist');
  });
});
