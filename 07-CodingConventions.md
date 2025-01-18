# Coding Conventions

You shouldn't overdo it with coding conventions. That's why I only use a minimum of rules.

## General

Indent is 4 spaces (tabs are avoided).

Allman style is preferred

```
while (x == y)
{
    foo();
    bar();
}
```

## Variables Naming

### Snake Case

| Format Example   | Variable Type                                |
| ---------------- | -------------------------------------------- |
| local_variable   | Variable with local scope.                   |
| function_param_  | A function parameter has tailing underscore. |
| m_member_var     | Basic type private member variable.          |
| mo_member_object | Complex object (class) member.               |
| s_member_var     | Basic type static private member variable.   |
| so_member_object | Complex static object member.                |

### Pascal Case

| Format Example | Variable Type                             |
| -------------- | ----------------------------------------- |
| PublicVariable | Variable with public or internal scope.   |
| PublicObject   | Object with public or internal scope.     |
| AnyMethod      | No difference between public and private. |

Walter Borst, Cuxhaven, 19.1.2025
