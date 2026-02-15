# PCGExtendedToolkitTest

Automation tests for [PCGExtendedToolkit](https://github.com/Nebukam/PCGExtendedToolkit). This is a development-only plugin — not intended for end users.

## Setup

Add as a submodule inside your project's `Plugins/` directory alongside PCGExtendedToolkit, and enable it in your `.uproject`:

```json
{
    "Name": "PCGExtendedToolkitTest",
    "Enabled": true
}
```

## Running Tests

Open the **Session Frontend** (`Window > Developer Tools > Session Frontend > Automation`), filter by `PCGEx`, and run.
