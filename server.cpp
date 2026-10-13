#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <climits>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "include/sqlite3.h"

using namespace std;

struct Edge { string to; int minutes; };
struct Route { int minutes = INT_MAX; vector<string> nodes; };
struct Candidate { string id, name, location, type; int eta; vector<string> route; };

string jsonEscape(const string& value) {
    string out = "\"";
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: if (c < 0x20) { char b[7]; snprintf(b, sizeof(b), "\\u%04x", c); out += b; } else out += static_cast<char>(c);
        }
    }
    out += '"'; return out;
}
string jsonArray(const vector<string>& values) {
    string out = "["; for (size_t i=0;i<values.size();++i) { if(i) out += ','; out += jsonEscape(values[i]); } return out + "]";
}
string jsonGet(const string& body, const string& key) {
    string needle = "\"" + key + "\""; size_t p = body.find(needle); if(p == string::npos) return "";
    p = body.find(':', p + needle.size()); if(p == string::npos) return ""; ++p;
    while(p<body.size() && isspace(static_cast<unsigned char>(body[p]))) ++p;
    if(p<body.size() && body[p]=='"') {
        ++p; string out; bool esc=false;
        for(;p<body.size();++p) { char c=body[p]; if(esc) { switch(c) { case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;default:out+=c; } esc=false; } else if(c=='\\') esc=true; else if(c=='"') break; else out+=c; }
        return out;
    }
    size_t e=p; while(e<body.size() && body[e]!=',' && body[e]!='}' && !isspace(static_cast<unsigned char>(body[e]))) ++e;
    return body.substr(p,e-p);
}
string httpResponse(const string& status, const string& type, const string& body) {
    return "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + to_string(body.size()) + "\r\nCache-Control: no-store\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Content-Type, Authorization\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\n\r\n" + body;
}

class DispatchEngine {
    sqlite3* db = nullptr;
    mutex m;
    map<string, vector<Edge>> graph;
    unordered_set<string> validSessions;
    int emergencyCounter = 1;

    bool exec(const string& sql) {
        char* err=nullptr; int rc=sqlite3_exec(db,sql.c_str(),nullptr,nullptr,&err);
        if(rc!=SQLITE_OK) { cerr << "SQLite: " << (err?err:sqlite3_errmsg(db)) << '\n'; sqlite3_free(err); return false; }
        return true;
    }
    bool prepare(const string& sql, sqlite3_stmt** stmt) { return sqlite3_prepare_v2(db,sql.c_str(),-1,stmt,nullptr)==SQLITE_OK; }
    void addRoad(const string& a,const string& b,int mins) { graph[a].push_back({b,mins}); graph[b].push_back({a,mins}); }
    void seed() {
        sqlite3_stmt* st=nullptr; prepare("SELECT COUNT(*) FROM roads",&st); int roadCount=0;
        if(st && sqlite3_step(st)==SQLITE_ROW) roadCount=sqlite3_column_int(st,0); if(st) sqlite3_finalize(st);
        if(roadCount>=65) return;
        exec("BEGIN IMMEDIATE"); exec("DELETE FROM roads");
        const char* roads = R"SQL(
INSERT OR IGNORE INTO roads(source,target,travel_time_mins) VALUES
('Kothrud_Stand','Karve_Road_Curve_1',2),('Karve_Road_Curve_1','Paud_Phata_Flyover',2),('Paud_Phata_Flyover','Sahyadri_Hospital_Node',2),('Sahyadri_Hospital_Node','Nal_Stop_Chowk',2),('Nal_Stop_Chowk','Garware_College_Bend',2),('Garware_College_Bend','Deccan_Goodluck_Chowk',2),('Deccan_Goodluck_Chowk','FC_Road_Entry',2),('FC_Road_Entry','Fergusson_College_Gate',2),('Fergusson_College_Gate','Sanjeevan_Hospital_Node',2),('Sanjeevan_Hospital_Node','Shivajinagar_Station',2),
('Deccan_Goodluck_Chowk','Sambhaji_Bridge_Entry',2),('Sambhaji_Bridge_Entry','Poona_Hospital_Node',2),('Poona_Hospital_Node','Alka_Talkies_Chowk',2),('Alka_Talkies_Chowk','Tilak_Road_Bend',2),('Tilak_Road_Bend','Swargate_Chowk',2),('Swargate_Chowk','Seven_Loves_Chowk',2),('Seven_Loves_Chowk','Camp_MG_Road',3),
('Karve_Road_Curve_1','Erandwane_Junction',2),('Erandwane_Junction','Nal_Stop_Chowk',2),('Erandwane_Junction','Law_College_Road_Bend',2),('Law_College_Road_Bend','Prabhat_Road_Junction',2),('Prabhat_Road_Junction','Deccan_Goodluck_Chowk',2),('Prabhat_Road_Junction','Bhandarkar_Road_Crossing',2),('Bhandarkar_Road_Crossing','Fergusson_College_Gate',2),
('FC_Road_Entry','JM_Road_Crossing',1),('JM_Road_Crossing','Deccan_Goodluck_Chowk',2),('JM_Road_Crossing','Sambhaji_Bridge_Entry',2),('Shivajinagar_Station','COEP_Junction',2),('COEP_Junction','Sambhaji_Bridge_Entry',2),('COEP_Junction','Civil_Court_Junction',2),('Civil_Court_Junction','Shivajinagar_Station',2),
('Poona_Hospital_Node','Mandai_Junction',2),('Mandai_Junction','Shukrawar_Peth_Crossing',2),('Shukrawar_Peth_Crossing','Tilak_Road_Bend',2),('Mandai_Junction','Swargate_Chowk',3),('Swargate_Chowk','Gultekdi_Junction',3),('Gultekdi_Junction','Seven_Loves_Chowk',2),('Camp_MG_Road','East_Street_Crossing',2),('East_Street_Crossing','Pune_Station_Junction',3),('Pune_Station_Junction','Bund_Garden_Road',3),('Bund_Garden_Road','Civil_Court_Junction',4),
('Sahyadri_Hospital_Node','Karve_Nagar_Bridge',3),('Karve_Nagar_Bridge','Warje_Road_Junction',4),('Warje_Road_Junction','Kothrud_Stand',5),('Nal_Stop_Chowk','Paud_Road_Junction',2),('Paud_Road_Junction','Kothrud_Stand',4),('East_Street_Crossing','Ruby_Hall_Junction',2),('Ruby_Hall_Junction','Pune_Station_Junction',2),('Ruby_Hall_Junction','Bund_Garden_Road',3),('Sanjeevan_Hospital_Node','Model_Colony_Crossing',3),('Model_Colony_Crossing','COEP_Junction',2),
('Karishma_Chowk','Karve_Road_Curve_1',2),('Karishma_Chowk','Ideal_Colony_Junction',2),('Ideal_Colony_Junction','Paud_Phata_Flyover',2),('MIT_Crossing','Paud_Phata_Flyover',2),('MIT_Crossing','Law_College_Road_Bend',2),('Nal_Stop_East_Crossing','Garware_College_Bend',2),('Nal_Stop_East_Crossing','Deccan_Gymkhana_Junction',2),('Deccan_Gymkhana_Junction','Deccan_Goodluck_Chowk',2),('Deccan_Gymkhana_Junction','Alka_Talkies_Chowk',2),('Shaniwar_Wada_Junction','Sambhaji_Bridge_Entry',2),('Shaniwar_Wada_Junction','Civil_Court_Junction',2),('Shaniwar_Wada_Junction','Laxmi_Road_Crossing',2),('Laxmi_Road_Crossing','Alka_Talkies_Chowk',2),('Laxmi_Road_Crossing','Rasta_Peth_Junction',2),('Rasta_Peth_Junction','Swargate_Chowk',2),('Rasta_Peth_Junction','Pune_Cantonment_Crossing',2),('Pune_Cantonment_Crossing','Camp_MG_Road',2),('Pune_Cantonment_Crossing','East_Street_Crossing',2),('Pune_Cantonment_Crossing','Koregaon_Park_Junction',3),('Koregaon_Park_Junction','Bund_Garden_Road',3);
)SQL";
        exec(roads);
        exec("INSERT OR IGNORE INTO ambulances(id,location,is_available,type) VALUES ('AMB-101','Kothrud_Stand',1,'ALS'),('AMB-102','Swargate_Chowk',1,'BLS'),('AMB-103','Camp_MG_Road',1,'ALS'),('AMB-104','Shivajinagar_Station',1,'ALS'),('AMB-105','Nal_Stop_Chowk',1,'BLS'),('AMB-106','Pune_Station_Junction',1,'BLS'),('AMB-107','Deccan_Goodluck_Chowk',1,'ALS');");
        exec("INSERT OR IGNORE INTO hospitals(id,name,location,specialties,available_beds,total_beds) VALUES ('H-201','Sahyadri Specialty Hospital','Sahyadri_Hospital_Node','Cardiac,Trauma,General',5,5),('H-202','Poona Hospital & Research Centre','Poona_Hospital_Node','General,Trauma',4,4),('H-203','Sanjeevan Cardiac Center','Sanjeevan_Hospital_Node','Cardiac,General',3,3),('H-204','City Trauma & Emergency Centre','Swargate_Chowk','Trauma,General',4,4),('H-205','Ruby Hall Clinic','Ruby_Hall_Junction','Cardiac,Trauma,General',5,5);");
        exec("COMMIT");
    }
    void loadGraph() {
        graph.clear(); sqlite3_stmt* st=nullptr;
        if(prepare("SELECT source,target,travel_time_mins FROM roads",&st)) while(sqlite3_step(st)==SQLITE_ROW) {
            string a=(const char*)sqlite3_column_text(st,0), b=(const char*)sqlite3_column_text(st,1); int t=sqlite3_column_int(st,2); addRoad(a,b,t);
        }
        if(st) sqlite3_finalize(st);
    }
    Route dijkstraUnlocked(const string& start,const string& target) {
        if(start==target) return {0,{start}};
        map<string,int> dist; map<string,string> parent;
        using Q=pair<int,string>; priority_queue<Q,vector<Q>,greater<Q>> pq;
        for(const auto& p:graph) dist[p.first]=INT_MAX;
        if(!dist.count(start)) dist[start]=INT_MAX; if(!dist.count(target)) dist[target]=INT_MAX;
        dist[start]=0; pq.push({0,start});
        while(!pq.empty()) { auto [d,u]=pq.top(); pq.pop(); if(d!=dist[u]) continue; if(u==target) break;
            for(const auto& e:graph[u]) if(d<=INT_MAX-e.minutes && d+e.minutes<dist[e.to]) { dist[e.to]=d+e.minutes; parent[e.to]=u; pq.push({dist[e.to],e.to}); }
        }
        if(dist[target]==INT_MAX) return {};
        vector<string> path; for(string at=target;;at=parent[at]) { path.push_back(at); if(at==start) break; if(!parent.count(at)) return {}; }
        reverse(path.begin(),path.end()); return {dist[target],path};
    }
public:
    DispatchEngine() {
        if(sqlite3_open("dispatch_system.db",&db)!=SQLITE_OK) { cerr<<"Cannot open SQLite database\n"; return; }
        sqlite3_busy_timeout(db,5000); exec("PRAGMA foreign_keys=ON");
        exec("CREATE TABLE IF NOT EXISTS roads(source TEXT NOT NULL,target TEXT NOT NULL,travel_time_mins INTEGER NOT NULL CHECK(travel_time_mins>0),PRIMARY KEY(source,target))");
        exec("CREATE TABLE IF NOT EXISTS ambulances(id TEXT PRIMARY KEY,location TEXT NOT NULL,is_available INTEGER NOT NULL DEFAULT 1,type TEXT NOT NULL)");
        exec("CREATE TABLE IF NOT EXISTS hospitals(id TEXT PRIMARY KEY,name TEXT NOT NULL,location TEXT NOT NULL,specialties TEXT NOT NULL,available_beds INTEGER NOT NULL,total_beds INTEGER NOT NULL DEFAULT 5)");
        // Migrate older project databases without deleting their audit records.
        bool hasTotalBeds=false; sqlite3_stmt* columns=nullptr;
        if(prepare("PRAGMA table_info(hospitals)",&columns)) while(sqlite3_step(columns)==SQLITE_ROW) { const char* col=(const char*)sqlite3_column_text(columns,1); if(col && string(col)=="total_beds") hasTotalBeds=true; }
        if(columns) sqlite3_finalize(columns);
        if(!hasTotalBeds) exec("ALTER TABLE hospitals ADD COLUMN total_beds INTEGER NOT NULL DEFAULT 5");
        exec("CREATE TABLE IF NOT EXISTS emergencies(id INTEGER PRIMARY KEY AUTOINCREMENT,request_id TEXT NOT NULL UNIQUE,patient_name TEXT NOT NULL,severity INTEGER NOT NULL CHECK(severity BETWEEN 1 AND 4),specialty TEXT NOT NULL,location TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'PENDING',assigned_ambulance TEXT,assigned_hospital TEXT,created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)");
        seed(); loadGraph();
        sqlite3_stmt* st=nullptr; if(prepare("SELECT COALESCE(MAX(id),0)+1 FROM emergencies",&st) && sqlite3_step(st)==SQLITE_ROW) emergencyCounter=sqlite3_column_int(st,0); if(st)sqlite3_finalize(st);
    }
    ~DispatchEngine(){ if(db) sqlite3_close(db); }
    bool login(const string& user,const string& pass,string& token) {
        lock_guard<mutex> lock(m); if(user!="controller" || pass!="MedEvac@123") return false;
        static mt19937_64 rng(random_device{}()); token=to_string(rng())+to_string(chrono::steady_clock::now().time_since_epoch().count()); validSessions.insert(token); return true;
    }
    bool authorized(const string& token) { lock_guard<mutex> lock(m); return !token.empty() && validSessions.count(token)>0; }
    string createEmergency(const string& requestId,const string& name,int severity,const string& specialty,const string& location) {
        lock_guard<mutex> lock(m);
        if(requestId.empty()||name.empty()||location.empty()||severity<1||severity>4) return "{\"status\":\"error\",\"message\":\"Please provide a patient/caller label, valid severity, location, and request ID.\"}";
        sqlite3_stmt* st=nullptr;
        if(!prepare("SELECT id,status FROM emergencies WHERE request_id=?",&st)) return "{\"status\":\"error\",\"message\":\"Database error\"}";
        sqlite3_bind_text(st,1,requestId.c_str(),-1,SQLITE_TRANSIENT);
        if(sqlite3_step(st)==SQLITE_ROW) { int id=sqlite3_column_int(st,0); string status=(const char*)sqlite3_column_text(st,1); sqlite3_finalize(st); return "{\"status\":\"success\",\"duplicate\":true,\"message\":\"This request was already registered\",\"id\":"+to_string(id)+",\"emergencyStatus\":"+jsonEscape(status)+"}"; }
        sqlite3_finalize(st);
        if(!graph.count(location)) return "{\"status\":\"error\",\"message\":\"Unknown incident location\"}";
        if(!exec("BEGIN IMMEDIATE")) return "{\"status\":\"error\",\"message\":\"Could not start database transaction\"}";
        if(!prepare("INSERT INTO emergencies(request_id,patient_name,severity,specialty,location) VALUES(?,?,?,?,?)",&st)) { exec("ROLLBACK"); return "{\"status\":\"error\",\"message\":\"Database error\"}"; }
        sqlite3_bind_text(st,1,requestId.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(st,2,name.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(st,3,severity); sqlite3_bind_text(st,4,specialty.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(st,5,location.c_str(),-1,SQLITE_TRANSIENT);
        bool ok=sqlite3_step(st)==SQLITE_DONE; sqlite3_finalize(st); int id=(int)sqlite3_last_insert_rowid(db);
        if(!ok || !exec("COMMIT")) { exec("ROLLBACK"); return "{\"status\":\"error\",\"message\":\"Could not save emergency\"}"; }
        return "{\"status\":\"success\",\"duplicate\":false,\"message\":\"Emergency registered for controller review\",\"id\":"+to_string(id)+",\"emergencyStatus\":\"PENDING\"}";
    }
    string state() {
        lock_guard<mutex> lock(m); string out="{\"status\":\"success\",\"nodes\":["; bool first=true;
        for(const auto& p:graph) { if(!first)out+=','; first=false; out+=jsonEscape(p.first); }
        out+="],\"emergencies\":["; first=true; sqlite3_stmt* st=nullptr;
        if(prepare("SELECT id,patient_name,severity,specialty,location,status,COALESCE(assigned_ambulance,''),COALESCE(assigned_hospital,''),created_at FROM emergencies WHERE status!='RESOLVED' ORDER BY CASE status WHEN 'PENDING' THEN 0 ELSE 1 END,severity ASC,id ASC",&st)) while(sqlite3_step(st)==SQLITE_ROW) {
            if(!first)out+=','; first=false;
            out+="{\"id\":"+to_string(sqlite3_column_int(st,0))+",\"patientName\":"+jsonEscape((const char*)sqlite3_column_text(st,1))+",\"severity\":"+to_string(sqlite3_column_int(st,2))+",\"specialty\":"+jsonEscape((const char*)sqlite3_column_text(st,3))+",\"location\":"+jsonEscape((const char*)sqlite3_column_text(st,4))+",\"status\":"+jsonEscape((const char*)sqlite3_column_text(st,5))+",\"ambulanceId\":"+jsonEscape((const char*)sqlite3_column_text(st,6))+",\"hospitalId\":"+jsonEscape((const char*)sqlite3_column_text(st,7))+",\"createdAt\":"+jsonEscape((const char*)sqlite3_column_text(st,8))+"}";
        } if(st)sqlite3_finalize(st);
        out+="],\"ambulances\":["; first=true;
        if(prepare("SELECT id,location,is_available,type FROM ambulances ORDER BY id",&st)) while(sqlite3_step(st)==SQLITE_ROW) { if(!first)out+=',';first=false; out+="{\"id\":"+jsonEscape((const char*)sqlite3_column_text(st,0))+",\"location\":"+jsonEscape((const char*)sqlite3_column_text(st,1))+",\"available\":"+(sqlite3_column_int(st,2)?string("true"):string("false"))+",\"type\":"+jsonEscape((const char*)sqlite3_column_text(st,3))+"}"; } if(st)sqlite3_finalize(st);
        out+="],\"hospitals\":["; first=true;
        if(prepare("SELECT id,name,location,specialties,available_beds,total_beds FROM hospitals ORDER BY id",&st)) while(sqlite3_step(st)==SQLITE_ROW) { if(!first)out+=',';first=false; out+="{\"id\":"+jsonEscape((const char*)sqlite3_column_text(st,0))+",\"name\":"+jsonEscape((const char*)sqlite3_column_text(st,1))+",\"location\":"+jsonEscape((const char*)sqlite3_column_text(st,2))+",\"specialties\":"+jsonEscape((const char*)sqlite3_column_text(st,3))+",\"availableBeds\":"+to_string(sqlite3_column_int(st,4))+",\"totalBeds\":"+to_string(sqlite3_column_int(st,5))+"}"; } if(st)sqlite3_finalize(st);
        return out+"]}";
    }
    string recommendations(int id) {
        lock_guard<mutex> lock(m); sqlite3_stmt* st=nullptr;
        if(!prepare("SELECT patient_name,severity,specialty,location,status FROM emergencies WHERE id=?",&st)) return "{\"status\":\"error\",\"message\":\"Database error\"}";
        sqlite3_bind_int(st,1,id); if(sqlite3_step(st)!=SQLITE_ROW) {sqlite3_finalize(st);return "{\"status\":\"error\",\"message\":\"Emergency not found\"}";}
        string name=(const char*)sqlite3_column_text(st,0), specialty=(const char*)sqlite3_column_text(st,2), loc=(const char*)sqlite3_column_text(st,3), status=(const char*)sqlite3_column_text(st,4); int severity=sqlite3_column_int(st,1); sqlite3_finalize(st);
        if(status!="PENDING") return "{\"status\":\"error\",\"message\":\"Recommendations are available only for pending emergencies\"}";
        vector<Candidate> ambs, hosps;
        if(prepare("SELECT id,location,type FROM ambulances WHERE is_available=1",&st)) while(sqlite3_step(st)==SQLITE_ROW) { string aid=(const char*)sqlite3_column_text(st,0), aloc=(const char*)sqlite3_column_text(st,1), type=(const char*)sqlite3_column_text(st,2); Route r=dijkstraUnlocked(aloc,loc); if(r.minutes!=INT_MAX) ambs.push_back({aid,aid,aloc,type,r.minutes,r.nodes}); } if(st)sqlite3_finalize(st);
        sort(ambs.begin(),ambs.end(),[](const Candidate&a,const Candidate&b){return a.eta==b.eta?a.id<b.id:a.eta<b.eta;});
        if(prepare("SELECT id,name,location,specialties,available_beds FROM hospitals WHERE available_beds>0",&st)) while(sqlite3_step(st)==SQLITE_ROW) { string hid=(const char*)sqlite3_column_text(st,0), hname=(const char*)sqlite3_column_text(st,1), hloc=(const char*)sqlite3_column_text(st,2), specs=(const char*)sqlite3_column_text(st,3); if(specs.find(specialty)==string::npos) continue; Route r=dijkstraUnlocked(loc,hloc); if(r.minutes!=INT_MAX) hosps.push_back({hid,hname,hloc,specs,r.minutes,r.nodes}); } if(st)sqlite3_finalize(st);
        sort(hosps.begin(),hosps.end(),[](const Candidate&a,const Candidate&b){return a.eta==b.eta?a.id<b.id:a.eta<b.eta;});
        auto encode=[](const vector<Candidate>& list,bool ambulance){string out="["; for(size_t i=0;i<list.size();++i){if(i)out+=',';const auto& c=list[i];out+="{\"id\":"+jsonEscape(c.id)+",\"name\":"+jsonEscape(c.name)+",\"location\":"+jsonEscape(c.location)+",\"eta\":"+to_string(c.eta)+",\"route\":"+jsonArray(c.route); if(ambulance)out+=",\"type\":"+jsonEscape(c.type); out+='}';} return out+"]";};
        return "{\"status\":\"success\",\"emergencyId\":"+to_string(id)+",\"patientName\":"+jsonEscape(name)+",\"severity\":"+to_string(severity)+",\"specialty\":"+jsonEscape(specialty)+",\"location\":"+jsonEscape(loc)+",\"ambulanceCandidates\":"+encode(ambs,true)+",\"hospitalCandidates\":"+encode(hosps,false)+"}";
    }
    string assign(int id,const string& ambulance,const string& hospital) {
        lock_guard<mutex> lock(m); sqlite3_stmt* st=nullptr;
        if(!exec("BEGIN IMMEDIATE"))return "{\"status\":\"error\",\"message\":\"Database transaction failed\"}";
        if(!prepare("SELECT location,status,specialty FROM emergencies WHERE id=?",&st)){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";}
        sqlite3_bind_int(st,1,id); if(sqlite3_step(st)!=SQLITE_ROW){sqlite3_finalize(st);exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Emergency not found\"}";}
        string loc=(const char*)sqlite3_column_text(st,0), status=(const char*)sqlite3_column_text(st,1), spec=(const char*)sqlite3_column_text(st,2);sqlite3_finalize(st);
        if(status!="PENDING"){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Emergency is no longer pending\"}";}
        if(!prepare("SELECT is_available FROM ambulances WHERE id=?",&st)){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";} sqlite3_bind_text(st,1,ambulance.c_str(),-1,SQLITE_TRANSIENT); bool ambOk=sqlite3_step(st)==SQLITE_ROW && sqlite3_column_int(st,0)==1;sqlite3_finalize(st);
        if(!ambOk){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Selected ambulance is no longer available. Refresh recommendations.\"}";}
        if(!prepare("SELECT available_beds,specialties FROM hospitals WHERE id=?",&st)){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";} sqlite3_bind_text(st,1,hospital.c_str(),-1,SQLITE_TRANSIENT); bool hospOk=false; if(sqlite3_step(st)==SQLITE_ROW) hospOk=sqlite3_column_int(st,0)>0 && string((const char*)sqlite3_column_text(st,1)).find(spec)!=string::npos;sqlite3_finalize(st);
        if(!hospOk){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Selected hospital is full or does not match the required specialty. Refresh recommendations.\"}";}
        Route ar=dijkstraUnlocked([&](){sqlite3_stmt* q=nullptr;string v="";if(prepare("SELECT location FROM ambulances WHERE id=?",&q)){sqlite3_bind_text(q,1,ambulance.c_str(),-1,SQLITE_TRANSIENT);if(sqlite3_step(q)==SQLITE_ROW)v=(const char*)sqlite3_column_text(q,0);sqlite3_finalize(q);}return v;}(),loc);
        Route hr=dijkstraUnlocked(loc,[&](){sqlite3_stmt* q=nullptr;string v="";if(prepare("SELECT location FROM hospitals WHERE id=?",&q)){sqlite3_bind_text(q,1,hospital.c_str(),-1,SQLITE_TRANSIENT);if(sqlite3_step(q)==SQLITE_ROW)v=(const char*)sqlite3_column_text(q,0);sqlite3_finalize(q);}return v;}());
        if(ar.minutes==INT_MAX||hr.minutes==INT_MAX){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Route unavailable\"}";}
        if(!prepare("UPDATE ambulances SET is_available=0 WHERE id=? AND is_available=1",&st)){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";}sqlite3_bind_text(st,1,ambulance.c_str(),-1,SQLITE_TRANSIENT);bool changed=sqlite3_step(st)==SQLITE_DONE&&sqlite3_changes(db)==1;sqlite3_finalize(st);if(!changed){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Ambulance was assigned concurrently; refresh recommendations.\"}";}
        if(!prepare("UPDATE hospitals SET available_beds=available_beds-1 WHERE id=? AND available_beds>0",&st)){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";}sqlite3_bind_text(st,1,hospital.c_str(),-1,SQLITE_TRANSIENT);sqlite3_step(st);bool bedChanged=sqlite3_changes(db)==1;sqlite3_finalize(st);if(!bedChanged){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Hospital capacity changed; refresh recommendations.\"}";}
        if(!prepare("UPDATE emergencies SET status='ASSIGNED',assigned_ambulance=?,assigned_hospital=? WHERE id=? AND status='PENDING'",&st)){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";}sqlite3_bind_text(st,1,ambulance.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,hospital.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_int(st,3,id);sqlite3_step(st);bool emChanged=sqlite3_changes(db)==1;sqlite3_finalize(st);if(!emChanged){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Emergency was changed concurrently\"}";}
        if(!exec("COMMIT")){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Could not commit assignment\"}";}
        return "{\"status\":\"success\",\"message\":\"Assignment reserved. Controller must confirm by phone.\",\"emergencyId\":"+to_string(id)+",\"ambulanceId\":"+jsonEscape(ambulance)+",\"hospitalId\":"+jsonEscape(hospital)+",\"ambulanceETA\":"+to_string(ar.minutes)+",\"ambulanceRoute\":"+jsonArray(ar.nodes)+",\"hospitalETA\":"+to_string(hr.minutes)+",\"hospitalRoute\":"+jsonArray(hr.nodes)+"}";
    }
    string assignmentDetails(int id) {
        lock_guard<mutex> lock(m); sqlite3_stmt* st=nullptr;
        if(!prepare("SELECT e.patient_name,e.severity,e.specialty,e.location,e.status,COALESCE(e.assigned_ambulance,''),COALESCE(e.assigned_hospital,''),COALESCE(a.location,''),COALESCE(h.name,''),COALESCE(h.location,'') FROM emergencies e LEFT JOIN ambulances a ON a.id=e.assigned_ambulance LEFT JOIN hospitals h ON h.id=e.assigned_hospital WHERE e.id=?",&st)) return "{\"status\":\"error\",\"message\":\"Database error\"}";
        sqlite3_bind_int(st,1,id); if(sqlite3_step(st)!=SQLITE_ROW){sqlite3_finalize(st);return "{\"status\":\"error\",\"message\":\"Emergency not found\"}";}
        string name=(const char*)sqlite3_column_text(st,0); int severity=sqlite3_column_int(st,1); string specialty=(const char*)sqlite3_column_text(st,2), loc=(const char*)sqlite3_column_text(st,3), status=(const char*)sqlite3_column_text(st,4), amb=(const char*)sqlite3_column_text(st,5), hosp=(const char*)sqlite3_column_text(st,6), ambLoc=(const char*)sqlite3_column_text(st,7), hospName=(const char*)sqlite3_column_text(st,8), hospLoc=(const char*)sqlite3_column_text(st,9); sqlite3_finalize(st);
        if(status!="ASSIGNED" && status!="CONFIRMED") return "{\"status\":\"error\",\"message\":\"Emergency has no active assignment\"}";
        Route ar=dijkstraUnlocked(ambLoc,loc), hr=dijkstraUnlocked(loc,hospLoc);
        return "{\"status\":\"success\",\"emergencyId\":"+to_string(id)+",\"patientName\":"+jsonEscape(name)+",\"severity\":"+to_string(severity)+",\"specialty\":"+jsonEscape(specialty)+",\"location\":"+jsonEscape(loc)+",\"dispatchStatus\":"+jsonEscape(status)+",\"ambulanceId\":"+jsonEscape(amb)+",\"hospitalId\":"+jsonEscape(hosp)+",\"hospitalName\":"+jsonEscape(hospName)+",\"ambulanceETA\":"+to_string(ar.minutes==INT_MAX?0:ar.minutes)+",\"ambulanceRoute\":"+jsonArray(ar.nodes)+",\"hospitalETA\":"+to_string(hr.minutes==INT_MAX?0:hr.minutes)+",\"hospitalRoute\":"+jsonArray(hr.nodes)+"}";
    }
    string confirm(int id,const string& driver,const string& hospital) {
        lock_guard<mutex> lock(m); if(driver!="yes"||hospital!="yes") return "{\"status\":\"error\",\"message\":\"Confirm both ambulance-team acceptance and hospital acceptance before confirming dispatch.\"}";
        sqlite3_stmt* st=nullptr; if(!prepare("UPDATE emergencies SET status='CONFIRMED' WHERE id=? AND status='ASSIGNED'",&st))return "{\"status\":\"error\",\"message\":\"Database error\"}";sqlite3_bind_int(st,1,id);sqlite3_step(st);bool ok=sqlite3_changes(db)==1;sqlite3_finalize(st);return ok?"{\"status\":\"success\",\"message\":\"Dispatch confirmed\"}":"{\"status\":\"error\",\"message\":\"Assignment not found or already confirmed\"}";
    }
    string resolve(int id) {
        lock_guard<mutex> lock(m); if(!exec("BEGIN IMMEDIATE"))return "{\"status\":\"error\",\"message\":\"Database error\"}";sqlite3_stmt* st=nullptr;
        if(!prepare("SELECT assigned_ambulance,assigned_hospital,status FROM emergencies WHERE id=?",&st)){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";}sqlite3_bind_int(st,1,id);if(sqlite3_step(st)!=SQLITE_ROW){sqlite3_finalize(st);exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Emergency not found\"}";}
        string amb=(const char*)sqlite3_column_text(st,0), hosp=(const char*)sqlite3_column_text(st,1), status=(const char*)sqlite3_column_text(st,2);sqlite3_finalize(st);if(status!="ASSIGNED"&&status!="CONFIRMED"){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Only assigned emergencies can be resolved\"}";}
        if(!prepare("UPDATE ambulances SET is_available=1 WHERE id=?",&st)){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";}sqlite3_bind_text(st,1,amb.c_str(),-1,SQLITE_TRANSIENT);sqlite3_step(st);sqlite3_finalize(st);
        if(!prepare("UPDATE hospitals SET available_beds=MIN(total_beds,available_beds+1) WHERE id=?",&st)){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";}sqlite3_bind_text(st,1,hosp.c_str(),-1,SQLITE_TRANSIENT);sqlite3_step(st);sqlite3_finalize(st);
        if(!prepare("UPDATE emergencies SET status='RESOLVED' WHERE id=?",&st)){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";}sqlite3_bind_int(st,1,id);sqlite3_step(st);sqlite3_finalize(st);if(!exec("COMMIT")){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Database error\"}";}return "{\"status\":\"success\",\"message\":\"Emergency resolved and resources released\"}";
    }
    string resetDemo() {
        lock_guard<mutex> lock(m); if(!exec("BEGIN IMMEDIATE"))return "{\"status\":\"error\",\"message\":\"Database error\"}";
        bool ok=exec("UPDATE ambulances SET is_available=1")&&exec("UPDATE hospitals SET available_beds=total_beds")&&exec("UPDATE emergencies SET status='RESOLVED' WHERE status!='RESOLVED'"); if(!ok||!exec("COMMIT")){exec("ROLLBACK");return "{\"status\":\"error\",\"message\":\"Reset failed\"}";}return "{\"status\":\"success\",\"message\":\"Demo resources reset; audit records retained\"}";
    }
};

DispatchEngine engine;
string headerValue(const string& request,const string& header) { size_t p=request.find(header); if(p==string::npos)return "";p+=header.size();size_t e=request.find("\r\n",p);return request.substr(p,e-p); }
void handleClient(int fd) {
    string request; char buf[4096]; size_t expected=0; bool headerDone=false;
    while(request.size()<1024*1024) { ssize_t n=recv(fd,buf,sizeof(buf),0); if(n<=0)break;request.append(buf,n);if(!headerDone){size_t p=request.find("\r\n\r\n");if(p!=string::npos){headerDone=true;string cl=headerValue(request,"Content-Length: ");try{expected=cl.empty()?0:stoul(cl);}catch(...){expected=0;} if(request.size()>=p+4+expected)break;}}else{size_t p=request.find("\r\n\r\n");if(p!=string::npos&&request.size()>=p+4+expected)break;}}
    string response; size_t sep=request.find("\r\n\r\n");string body=sep==string::npos?"":request.substr(sep+4);size_t lineEnd=request.find("\r\n");string first=request.substr(0,lineEnd);istringstream iss(first);string method,path,version;iss>>method>>path>>version;
    if(method=="OPTIONS") response=httpResponse("204 No Content","text/plain","");
    else if(method=="GET"&&(path=="/"||path=="/index.html")) {ifstream f("static/index.html");if(f){stringstream ss;ss<<f.rdbuf();response=httpResponse("200 OK","text/html; charset=UTF-8",ss.str());}else response=httpResponse("404 Not Found","text/plain","Missing static/index.html");}
    else if(method=="POST"&&path=="/api/login") {string token;bool ok=engine.login(jsonGet(body,"username"),jsonGet(body,"password"),token);response=httpResponse(ok?"200 OK":"401 Unauthorized","application/json",ok?"{\"status\":\"success\",\"token\":"+jsonEscape(token)+",\"role\":\"controller\"}":"{\"status\":\"error\",\"message\":\"Invalid controller credentials\"}");}
    else if(path.rfind("/api/",0)==0) {
        string auth=headerValue(request,"Authorization: ");string token=auth.rfind("Bearer ",0)==0?auth.substr(7):"";
        if(!engine.authorized(token)) response=httpResponse("401 Unauthorized","application/json","{\"status\":\"error\",\"message\":\"Please sign in as a controller\"}");
        else if(method=="GET"&&path=="/api/state") response=httpResponse("200 OK","application/json",engine.state());
        else if(method=="POST"&&path=="/api/emergency") {int sev=1;try{sev=stoi(jsonGet(body,"severity"));}catch(...){}response=httpResponse("200 OK","application/json",engine.createEmergency(jsonGet(body,"requestId"),jsonGet(body,"patientName"),sev,jsonGet(body,"requiredSpecialty"),jsonGet(body,"location")));}
        else if(method=="POST"&&path=="/api/recommendations") {int id=0;try{id=stoi(jsonGet(body,"emergencyId"));}catch(...){}response=httpResponse("200 OK","application/json",engine.recommendations(id));}
        else if(method=="POST"&&path=="/api/assign") {int id=0;try{id=stoi(jsonGet(body,"emergencyId"));}catch(...){}response=httpResponse("200 OK","application/json",engine.assign(id,jsonGet(body,"ambulanceId"),jsonGet(body,"hospitalId")));}
        else if(method=="POST"&&path=="/api/confirm") {int id=0;try{id=stoi(jsonGet(body,"emergencyId"));}catch(...){}response=httpResponse("200 OK","application/json",engine.confirm(id,jsonGet(body,"driverAccepted"),jsonGet(body,"hospitalAccepted")));}
        else if(method=="POST"&&path=="/api/assignment-details") {int id=0;try{id=stoi(jsonGet(body,"emergencyId"));}catch(...){}response=httpResponse("200 OK","application/json",engine.assignmentDetails(id));}
        else if(method=="POST"&&path=="/api/resolve") {int id=0;try{id=stoi(jsonGet(body,"emergencyId"));}catch(...){}response=httpResponse("200 OK","application/json",engine.resolve(id));}
        else if(method=="POST"&&path=="/api/reset") response=httpResponse("200 OK","application/json",engine.resetDemo());
        else response=httpResponse("404 Not Found","application/json","{\"status\":\"error\",\"message\":\"Unknown API route\"}");
    } else response=httpResponse("404 Not Found","text/plain","Route Not Found");
    size_t sent=0;while(sent<response.size()){ssize_t n=send(fd,response.data()+sent,response.size()-sent,0);if(n<=0)break;sent+=n;}close(fd);
}
int main(){ios::sync_with_stdio(false);cin.tie(nullptr);int server=socket(AF_INET,SOCK_STREAM,0);if(server<0){cerr<<"Socket creation failed\n";return 1;}int opt=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_ANY);addr.sin_port=htons(18080);if(::bind(server,(sockaddr*)&addr,sizeof(addr))<0){cerr<<"Port 18080 is busy or unavailable\n";close(server);return 1;}if(listen(server,32)<0){cerr<<"Listen failed\n";close(server);return 1;}cout<<"MedEvac controller portal running at http://localhost:18080\nDemo login: controller / MedEvac@123\n";while(true){int client=accept(server,nullptr,nullptr);if(client>=0)thread(handleClient,client).detach();}close(server);return 0;}
