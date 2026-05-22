# Mock Orchestration Environment

The mock environment validates Hub behavior before hardware exists. It consumes
normalized Hub Protocol messages and never bypasses the Bridge boundary.

## Flow coverage

- `AutomationEngine` and `RuleEngine` exercise state, event, and stream rules.
- `StreamAnalysisManager` attaches mock analyzers to `stream.available`
  metadata from `StreamRegistry`.
- `MockVisionAnalyzer` emits deterministic `analysis.result` messages for
  person, occupancy, object, and package scenarios.
- `OccupancyAnalyzer` aggregates simulated motion and analysis signals into
  Hub-local occupancy confidence.
- `OrchestrationSimulator` produces deterministic Bridge traffic for workflows.
- `IntegrationTester` and `WorkflowTester` verify event fanout, command routing,
  state cache synchronization, occupancy tracking, and stream lifecycle.

## Debugging

- `EventTracer` keeps an internal orchestration timeline.
- `OrchestrationConsole` logs event, automation, analysis, command, stream, and
  subscription lines.
- `StateDebugger` summarizes cache, occupancy, streams, automations, and
  subscriptions for scenario snapshots.

The regular Hub startup wires the mock subscribers onto the internal
`SubscriptionManager`. Incoming mock ecosystem traffic can be posted to
`POST /hub/messages`, and outbound simulated commands/results continue to leave
through `HubClient` as Hub Protocol envelopes.
