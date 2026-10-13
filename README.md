# MedEvac — Controller-led Emergency Dispatch Prototype

MedEvac is a C++17 + SQLite academic prototype for a centralized emergency controller. It is a decision-support demonstration, not a medically validated or production-ready dispatch service.

## Run locally (macOS / Linux)

Requirements: a C++17 compiler (`g++`), C compiler (`cc`), pthreads, and a browser with internet access for Leaflet and OpenStreetMap tiles.

```bash
chmod +x run_portal.command
./run_portal.command
```

Then open **http://localhost:18080** if the browser does not open automatically.

### Demo controller credentials

- Username: `controller`
- Password: `MedEvac@123`

These are demo credentials. Do not use them for a real deployment.

## Workflow

1. The controller receives a phone call and enters a short incident record.
2. The server persists the case with a unique request ID; a retry of the same request ID returns the existing case instead of duplicating it.
3. The engine ranks available ambulances by Dijkstra road-network travel time and ranks hospitals by specialty, available beds, reachability, and travel time.
4. The controller chooses an ambulance and hospital. The server rechecks resource availability and commits resource reservation plus case assignment in a SQLite transaction.
5. The controller calls the ambulance team and hospital. The dispatch is confirmed only after both confirmation checkboxes are acknowledged.
6. The controller can close the demonstration case and release resources.

## Core-subject mapping

- **DSA:** adjacency-list road graph, Dijkstra shortest paths, priority queues for shortest-path exploration and candidate ranking.
- **DBMS:** SQLite persistence, unique request IDs, parameterized SQL statements, transaction-based assignment, resource capacity constraints.
- **Operating Systems / Networking:** POSIX TCP sockets, per-client threads, mutex-protected shared engine state.
- **OOP:** `DispatchEngine` encapsulates database, graph, authentication sessions, routing, and dispatch operations; route/candidate structures model domain data.
- **Mathematics:** weighted graph optimization and travel-time minimization.

## Important implementation notes

- The Pune-area nodes, road weights, ambulances, hospitals, ETAs, and bed capacities are illustrative seed data for demonstration. They are not live data or reliable real-world travel estimates.
- Leaflet and OpenStreetMap tiles are loaded from external services, so map tiles require internet access.
- This demo has one controller role. The login endpoint issues an in-memory bearer token for API requests. Sessions expire when the server restarts; this is not a production authentication design.
- The reset action preserves the audit history and marks active demo cases resolved while restoring demo resource availability.
- The server binds to port 18080. Stop it with Ctrl+C. If the port is occupied, stop the previous server process before restarting.

## API overview

- `POST /api/login`
- `GET /api/state` (bearer token required)
- `POST /api/emergency`
- `POST /api/recommendations`
- `POST /api/assign`
- `POST /api/confirm`
- `POST /api/resolve`
- `POST /api/reset`
