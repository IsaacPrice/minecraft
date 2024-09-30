# Code Standards and Quality

## Contents
1. [Introduction](#introduction)
2. [Naming Conventions and Standards](#naming-conventions-and-standards)
3. [Indentation and Spacing](#indentation-and-spacing)
4. [Good Programming Practices](#good-programming-practices)
5. [Architecture](#architecture)
6. [Comments](#comments)
7. [Exception Handling](#exception-handling)
8. [Debugging and Troubleshooting](#debugging-and-troubleshooting)

---

## Introduction
This document outlines coding standards that aim to create consistent, readable, and maintainable code across projects.

## Naming Conventions and Standards

### Use:
- **Pascal casing** for file names, namespaces, types, interfaces, methods, properties, and events.
  - Example: `TestFile.cs`, `SomeNamespace`, `ISomeReader`
- **ALL CAPS** for constants.
  - Example: `public const int MIN_VALUE = 0;`
- **Camel casing** for variables and method parameters.
  - Example: `int totalCount = 0;`
- Prefix **I** for interfaces using Pascal Casing.
  - Example: `ITestClass`
- Descriptive names for variables.
  - Example: `string address; int salary;`
- Use **underscores** for private member variables.
  - Example: `private string _connectionInformation;`
- Prefix **is**, **can**, or **has** for boolean variables.
  - Example: `bool isFinished;`
- Namespace structure: `<company>.<product>.<module>.<submodule>`.
- Use appropriate prefixes for UI elements.
- Match file names with class names.

### Do Not Use:
- Abbreviations for variable names.
- Hungarian notation.
  - Example: `string m_sName;`
- Single character variable names.
- Underscores for local variable names.

## Indentation and Spacing

- Use **Tabs** for indentation.
- Comments should align with code indentation.
  - Example:
    ```csharp
    // Comment
    string message = "Hello";
    ```
- Curly braces should be on a new line and aligned with surrounding code.
  - Example:
    ```csharp
    if (...) 
    {
        // Code
    }
    ```
- Use **one blank line** to separate logical groups of code.
- Use **two blank lines** between methods.
- Organize file content in the following order:
  - Public Constants
  - Private Constants
  - Private Static Variables
  - Private Instance Variables
  - Constructors
  - Public Methods
  - Private Methods
  - Public Properties

## Good Programming Practices

- **Descriptive method names**: A method name should reflect its functionality.
- A method should perform **only one job**.
- Always account for **unexpected values** in condition checks.
- **Avoid hard-coding** numbers and strings; use constants or resources instead.
- Convert strings to **lowercase** before comparing.
  - Example:
    ```csharp
    if (name.ToLower() == "john") { ... }
    ```
- Use **String.Empty** instead of `""`.
- **Minimize use of member variables**: Pass variables between methods instead.
- Use **enums** for discrete values instead of numbers or strings.
- Do not make member variables public or protected; use private with properties.
- Avoid **hard-coding paths**; retrieve them programmatically.
- Implement a **logging system** with varying levels (errors, warnings, traces).
- Ensure **proper resource management** with "using" blocks or try-finally.

## Architecture

- Use a **multi-layer architecture** with separate layers for Database, Business Logic, Service/API, and UI.
- **Never access the database** from the UI directly; use a data layer.
- Use **try-catch** for exception handling, and ensure exceptions are properly logged.
- Organize your application into **multiple assemblies** based on functionality.

## Comments

- Code should be **self-documenting**. Write comments only where necessary.
- Use proper grammar and punctuation in comments.
- Avoid over-commenting when the code is easily understandable.

## Exception Handling

- **Never hide exceptions**. Always log them with detailed information.
- Catch **specific exceptions** rather than generic ones.
- Maintain the **original stack trace** when throwing exceptions.
  - Example:
    ```csharp
    catch { throw; }
    ```
- Use **custom exceptions** where appropriate to make code more understandable.
- Avoid adding try-catch blocks everywhere—use them only where needed.

## Debugging and Troubleshooting

- Utilize a **standardized debugging decision tree** to diagnose issues quickly and effectively.
