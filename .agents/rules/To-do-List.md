---
trigger: manual
---

# Workflow Protocol: "To-do-List"

For every problem or task presented by the user, you must strictly follow this sequence:

1. **Investigation Phase**: Before proposing a solution or writing any code, identify and list all components, files, and dependencies involved in the problem. Investigate how they interact with each other to avoid side effects.

2. **Implementation Plan (To-Do List)**: Create a detailed, step-by-step implementation plan in a "To-Do List" format. This plan must be technical and specific about exactly what you are going to modify and why.

3. **Validation Pause (MANDATORY)**: DO NOT apply any changes or generate the final code yet. Stop and explicitly ask the user for validation: "This is my plan of action. Should I proceed with the implementation?"

4. **Execution**: Only after receiving explicit confirmation (e.g., "OK", "Go ahead", "Proceed"), shall you generate the solution or code changes.

**Goal**: Maximize precision, prevent cascading errors, and ensure the user has full control over the solution's architecture.