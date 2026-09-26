Repository-specific CI package for AquaMQTT V5.

Prerequisite:
Edit example-aquamqtt-v5.yaml and change external_components to:

external_components:
  - source:
      type: local
      path: ./components
    components: [aquamqtt]

Then commit this workflow.
