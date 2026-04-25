# v8 Implementation Checklist

## Phase 1: Documentation First
- [x] Create v8 README.
- [x] Create feature inventory.
- [x] Create pin map.
- [x] Create boot sequence doc.
- [x] Create config precedence doc.
- [x] Create Firebase design doc.
- [x] Create rate limit design doc.
- [x] Create number rules doc.
- [x] Create API endpoints doc.
- [x] Create logging standard doc.
- [x] Create copilot instructions file at repo root.

### Acceptance
- All docs exist and agree on scope.
- All docs reflect the Firebase-first direction.
- All docs explicitly exclude MQTT.

## Phase 2: Scaffold
- [x] Create v8 runtime folder structure.
- [x] Add class and module stubs.
- [x] Add dashboard asset placeholders.
- [x] Add config/secrets template files.

### Acceptance
- Files exist with clean interfaces.
- No business logic yet, only safe scaffolding.

## Phase 3: Execution Readiness
- [x] Validate docs against the baseline.
- [x] Confirm Firebase backend choice and schema.
- [x] Confirm command flow and counter flow.

### Acceptance
- Implementation can begin without further architecture changes.
