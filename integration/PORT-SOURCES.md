# Settings port sources

Base frameworks/base HEAD: `198fa72864710315a791ca620ffb2f6c823b6054`.

The integration patch includes the previous framework fixes and the settings work; apply it to the exact base above.

Upstream commits used as references; originals retain their copyright notices:

- 9dda482da53d94b4f32d85577b4f7b31bba8858e — SystemUI: Add customization to show battery percent — Pranav Vashi
- 0762dbd729f534c6f2b9b9ad04df464af032b5e2 — SystemUI: Bring back good ol' circle battery style once again — LuK1337
- ee79fef38eac0534bc899d9e84ce3b1d6d4ba791 — SystemUI: Clock position customization — Luca Stefani
- 3775edea4b83b3f9ec297099c49302e269093e5c — SystemUI: Allow to change brightness slider positioning in new compose QS — Abhay Singh Gill
- ad39fa1ed1a546e6f653c50ac7860794902f6938 — SystemUI: Add auto brightness toggle in brightness slider in new compose QS — rmp22

Adaptations: current HydratedActivatable APIs, active native clock layout, SDK network traffic manager, current QS layout and restrictions, observer cleanup, dynamic clock/carrier padding.

The old 23.2 brightness repository/interactor files were not restored: 24.0 moved them into pods. The auto-brightness control uses existing system settings and the current slider implementation.

Validation: local device screenshots and setting restoration, plus production SystemUI builds. The full ROM built successfully and booted on houji (1789213062); targeted settings validation passed. The multivalent BatteryRepositoryTest still references the old Android percentage interface and requires migration; its test suite has not been run.
