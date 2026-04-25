# v8 Implementation Checklist

## Phase 1: Documentation First
- [ ] Create v8 README.
- [ ] Create feature inventory.
- [ ] Create pin map.
- [ ] Create boot sequence doc.
- [ ] Create config precedence doc.
- [ ] Create Firebase design doc.
- [ ] Create rate limit design doc.
- [ ] Create number rules doc.
- [ ] Create API endpoints doc.
- [ ] Create logging standard doc.
- [ ] Create copilot instructions file at repo root.

### Acceptance
- All docs exist and agree on scope.
- All docs reflect the Firebase-first direction.
- All docs explicitly exclude MQTT.

## Phase 2: Scaffold
- [ ] Create v8 runtime folder structure.
- [ ] Add class and module stubs.
- [ ] Add dashboard asset placeholders.
- [ ] Add config/secrets template files.

### Acceptance
- Files exist with clean interfaces.
- No business logic yet, only safe scaffolding.

## Phase 3: Execution Readiness
- [ ] Validate docs against the baseline.
- [ ] Confirm Firebase backend choice and schema.
- [ ] Confirm command flow and counter flow.

### Acceptance
- Implementation can begin without further architecture changes.
