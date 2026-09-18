# Info
Package is provided by Loupe  
https://loupe.team  
info@loupe.team  
1-800-240-7042  

# Description

This repo provides packages for communicating with a B&R PLC using WebSockets and a JSON interface. This allows the PLC to act as a server for a Loupe UX client. 

### Library Package
The OMJSON library package bundles up the function blocks needed in order to run the WebSocket server and read/write to PLC variables.  

### Program Package
This repo also contains a program package that can be deployed to run the OMJSON-based webserver.

# Installation

To install the program package using the Loupe Package Manager (LPM), in an initialized Automation Studio project directory run `lpm install luxprog`. Note that this will also pull in the library package as a dependency. 
If you only want to install the library package, run `lpm install omjson`. 

For more information about LPM, see https://loupeteam.github.io/LoupeDocs/tools/lpm.html.

# Security

The servers in this library have **no authentication and no encryption**. Any client that can open a TCP connection to the server port (default 8000) can send requests, and a browser will let any web page open a WebSocket to the PLC. By default, clients can read **and write** every variable on the PLC by name.

Before using this on a machine:

### 1. Restrict which variables clients can access

Pass an access list to the server. Variables that are not covered by an entry can neither be read nor written. An entry covers the variable and everything below it (structure members, array elements). If more than one entry matches, the most specific one wins.

```
VAR
	hmiAccess : jsonAccess_typ;
	hmiAccessList : ARRAY[0..2] OF jsonAccessEntry_typ;
END_VAR

hmiAccessList[0].name := 'gHmi';            hmiAccessList[0].access := JSON_ACCESS_READ;
hmiAccessList[1].name := 'gHmi.cmd';        hmiAccessList[1].access := JSON_ACCESS_READWRITE;
hmiAccessList[2].name := 'gHmi.par';        hmiAccessList[2].access := JSON_ACCESS_READWRITE;

hmiAccess.pEntries := ADR(hmiAccessList);
hmiAccess.numEntries := 3;
jsonWebSocketServer_0.pAccess := ADR(hmiAccess);
```

- Names are case sensitive and must be written the way the client requests them (`'gVar'`, `'Task:var'`, `'gVar.member'`, `'gArray[2]'`).
- `'gHmi'` covers `gHmi.cmd.start` and `gHmi[0]`, but not `gHmiOther`. Reading `gHmi` when only `gHmi.cmd` is listed is denied.
- A denied read returns `"undefined"`, the same as a variable that does not exist. A denied write is skipped; other values in the same request are still written.
- `hmiAccess.deniedReadCount`, `deniedWriteCount`, `lastDeniedName` and `lastDeniedClientIP` show what was rejected, and the server reports `JSON_ERR_ACCESSDENIED`.
- `hmiAccess.disableWrites := TRUE` makes the server read only. `hmiAccess.allowedClientIP[]` limits which clients may connect.
- **If `pAccess` is 0 there are no restrictions.** The server output `AccessControlActive` is FALSE in that case; consider raising an alarm on it. With `pAccess` set and `numEntries = 0`, everything is denied.

When using the luxprog program package, fill `gLuxAccessList[]` from your application instead. As long as `gLuxAccessList[0]` is empty, all variables are accessible.

### 2. Expose an interface structure, not your machine variables

Give the HMI its own structure (for example `gHmi.status`, `gHmi.par`, `gHmi.cmd`) and list only that. Let PLC code copy status into it, and validate and limit parameters and commands before passing them on to the machine. That way a client can never put a value into the control logic that the PLC did not check first. Do not put pointers or references into this structure.

### 3. Restrict the network

- Set `ServerIP` to the interface the HMI is connected to. If it is empty the server listens on all interfaces.
- Add a firewall rule in the Automation Runtime configuration that only allows the HMI to reach the server port.
- If the PLC is reachable from an office network or remotely, put a gateway that provides TLS and a login in front of it, or use OPC UA for that connection. An access list limits what a client can do; it does not tell you who the client is.

# Documentation
For more documentation and examples, see https://loupeteam.github.io/LoupeDocs/libraries/omjson.html (or you can run `lpm docs omjson`).

# Licensing

This project is licensed under the [MIT License](LICENSE).