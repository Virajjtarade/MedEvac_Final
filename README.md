# MedEvac — Emergency Dispatch Coordination Prototype

MedEvac is an academic C++17/SQLite prototype demonstrating a controller-led emergency workflow. It is a **decision-support demonstration only**; it is not a medically validated, production-secure, or real emergency dispatch system.

## Run locally (macOS / Linux)

Requirements: C++17 compiler (`g++`), C compiler (`cc`), POSIX threads, and a browser with internet access for Leaflet/OpenStreetMap tiles.

```bash
chmod +x run_portal.command
./run_portal.command
```

Open `http://localhost:18080` if the browser does not open automatically.

- Controller: `http://localhost:18080/`
- Driver: `http://localhost:18080/driver`
- Hospital demo: `http://localhost:18080/hospital`

### Demonstration credentials

- Controller: `controller` / `MedEvac@123`
- Driver: `AMB-101` / `Driver@123`
- Other seeded ambulance IDs may use the same demo driver password.

These credentials are fixed demonstration credentials, not suitable for deployment. Do not reuse them outside a classroom/demo environment.

## Optional same-Wi-Fi multi-device demonstration

By default, the server binds to loopback only. To test from another device on the **same trusted Wi-Fi network**, start it with LAN mode explicitly enabled:

```bash
MEDEVAC_LAN=1 ./run_portal.command
```

Find the host computer's private LAN IP address, then open `http://HOST-LAN-IP:18080` from the second device. Both devices must reach the same running server and shared SQLite database. Allow the port only on a trusted private network and turn LAN mode off after testing.

**Security warning:** LAN mode uses plain HTTP and fixed demo credentials. This is for controlled classroom testing only. Do not port-forward this service or expose it directly to the public internet. Production use would require HTTPS, properly hashed per-user passwords, session expiry/revocation, rate limiting, role-specific authorization for all endpoints, protected hospital access, audit controls, backups, monitoring, and a security review.

## Demonstration workflow

1. The controller receives a call and records an emergency using an illustrative road-network node.
2. The server stores the request in SQLite and uses a unique request ID to avoid duplicate creation on retries.
3. The route engine ranks available ambulances and matching hospitals using Dijkstra shortest paths over a weighted, simulated graph.
4. The controller selects an ambulance and hospital. The server rechecks availability and reserves the ambulance and hospital capacity within a database transaction.
5. The controller records telephone confirmation from the ambulance team and hospital before confirming dispatch.
6. Driver status updates appear in the controller's notification panel; the hospital portal displays shared demo assignments and capacity.
7. Closing a case releases demo resources.

Batch allocation processes pending emergencies in priority order and attempts to use distinct available ambulances and available hospital capacity. Requests that cannot be allocated remain for manual review.

## Technology and subject mapping

- **C++17:** backend server, request handling, graph algorithms and dispatch logic.
- **POSIX TCP sockets:** HTTP request/response transport between browsers and the server.
- **SQLite:** persistent emergency, ambulance, hospital and road records.
- **SQL prepared statements:** parameter binding for user-supplied values in database operations.
- **Dijkstra's algorithm:** shortest path over a weighted adjacency-list graph.
- **Transactions:** assignment operations reserve multiple resources consistently or roll back on failure.
- **HTML/CSS/JavaScript + Leaflet:** separate browser portals and map visualization.

## Road network and route limitations

The Pune-area road graph contains additional junction and approach-road nodes to make route polylines more detailed. Coordinates, road weights, locations, ETAs, ambulance availability, hospital specialties and bed capacities are illustrative seed data. They are **not live traffic, verified road geometry, GPS tracking, or reliable real-world travel estimates**. Map tiles require internet access.

## API overview

- `POST /api/login`
- `POST /api/driver-login`
- `GET /api/driver/assignments` (driver bearer token required)
- `POST /api/driver/status` (driver bearer token required)
- `GET /api/state` (controller bearer token required)
- `POST /api/emergency`
- `POST /api/recommendations`
- `POST /api/assign`
- `POST /api/confirm`
- `POST /api/resolve`
- `POST /api/reset`
- `GET /api/hospital-state` (demo/read-only data endpoint; not suitable for sensitive real hospital information)

## Known limitations and next steps

- Fixed demo credentials and in-memory sessions; session expiry/revocation and real identity management are not implemented.
- HTTP is not encrypted. LAN mode should be used only on a trusted demonstration network.
- Hospital demo data endpoint is not a production access-control model.
- The road graph is synthetic and does not consume live traffic or GPS feeds.
- Browser polling refreshes shared status periodically; this is not guaranteed low-latency real-time messaging.
- No external hospital information system, ambulance GPS unit, SMS gateway, or emergency service is connected.
- Before any real deployment: add secure password storage, rate limiting, session lifecycle management, comprehensive role-based authorization, TLS, audit logging, automated tests, backups, observability, and a formal safety/security review.

Do not use this prototype to make real clinical or dispatch decisions.
