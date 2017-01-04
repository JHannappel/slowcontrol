--
-- PostgreSQL database dump
--

SET statement_timeout = 0;
SET lock_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SET check_function_bodies = false;
SET client_min_messages = warning;

--
-- Name: plpgsql; Type: EXTENSION; Schema: -; Owner: 
--

CREATE EXTENSION IF NOT EXISTS plpgsql WITH SCHEMA pg_catalog;


--
-- Name: EXTENSION plpgsql; Type: COMMENT; Schema: -; Owner: 
--

COMMENT ON EXTENSION plpgsql IS 'PL/pgSQL procedural language';


SET search_path = public, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;

--
--

CREATE TABLE comments (
    uid integer,
    comment text,
    "time" timestamp with time zone DEFAULT now()
);



--
--

CREATE TABLE compound_families (
    parent_id integer NOT NULL,
    child_id integer NOT NULL,
    child_name text
);



--
--

CREATE TABLE compound_list (
    name text NOT NULL,
    description text,
    id integer NOT NULL
);



--
--

CREATE SEQUENCE compound_list_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE compound_list_id_seq OWNED BY compound_list.id;


--
--

CREATE TABLE compound_uids (
    id integer NOT NULL,
    uid integer NOT NULL,
    child_name text
);



--
--

CREATE TABLE daemon_heartbeat (
    daemonid integer,
    daemon_time timestamp with time zone,
    server_time timestamp with time zone DEFAULT now(),
    next_beat timestamp with time zone
)
WITH (fillfactor='10');



--
--

CREATE TABLE daemon_list (
    daemonid integer NOT NULL,
    description text NOT NULL,
    name text,
    host text
);



--
--

CREATE SEQUENCE daemon_list_daemonid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE daemon_list_daemonid_seq OWNED BY daemon_list.daemonid;


--
--

CREATE TABLE measurements_bool (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value boolean
);



--
--

CREATE TABLE measurements_float (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value real NOT NULL
);



--
--

CREATE TABLE measurements_int2 (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value smallint
);



--
--

CREATE TABLE measurements_int4 (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value integer
);



--
--

CREATE TABLE measurements_int8 (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value bigint
);



--
--

CREATE TABLE measurements_trigger (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL
);



--
--

CREATE TABLE setvalue_requests (
    uid integer,
    request text NOT NULL,
    response text,
    request_time timestamp with time zone DEFAULT now(),
    response_time timestamp with time zone,
    id integer NOT NULL,
    comment text,
    result boolean
)
WITH (fillfactor='30');



--
--

CREATE SEQUENCE setvalue_requests_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE setvalue_requests_id_seq OWNED BY setvalue_requests.id;


--
--

CREATE TABLE site_links (
    url text,
    name text,
    context text,
    number integer
);



--
--

CREATE TABLE state_types (
    typename text NOT NULL,
    explanation text,
    type integer NOT NULL,
    class text
);



--
--

CREATE SEQUENCE state_types_type_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE state_types_type_seq OWNED BY state_types.type;


--
--

CREATE TABLE time_intervals (
    tdiff interval,
    name text
);



--
--

CREATE TABLE uid_config_history (
    uid integer,
    name text,
    value text,
    comment text,
    valid_from timestamp with time zone,
    valid_to timestamp with time zone DEFAULT now()
);



--
--

CREATE TABLE uid_configs (
    uid integer NOT NULL,
    name text NOT NULL,
    value text,
    comment text,
    last_change timestamp with time zone DEFAULT now()
);



--
--

CREATE TABLE uid_daemon_connection (
    uid integer,
    daemonid integer
);



--
--

CREATE TABLE uid_list (
    description text NOT NULL,
    uid integer NOT NULL,
    data_table text,
    is_write_value boolean DEFAULT false
);



--
--

CREATE SEQUENCE uid_list_uid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE uid_list_uid_seq OWNED BY uid_list.uid;


--
--

CREATE TABLE uid_state_history (
    uid integer,
    type integer,
    valid_from timestamp with time zone,
    valid_to timestamp with time zone,
    reason text
);



--
--

CREATE TABLE uid_states (
    uid integer NOT NULL,
    type integer,
    valid_from timestamp with time zone,
    reason text
);



--
--

ALTER TABLE ONLY compound_list ALTER COLUMN id SET DEFAULT nextval('compound_list_id_seq'::regclass);


--
--

ALTER TABLE ONLY daemon_list ALTER COLUMN daemonid SET DEFAULT nextval('daemon_list_daemonid_seq'::regclass);


--
--

ALTER TABLE ONLY setvalue_requests ALTER COLUMN id SET DEFAULT nextval('setvalue_requests_id_seq'::regclass);


--
--

ALTER TABLE ONLY state_types ALTER COLUMN type SET DEFAULT nextval('state_types_type_seq'::regclass);


--
--

ALTER TABLE ONLY uid_list ALTER COLUMN uid SET DEFAULT nextval('uid_list_uid_seq'::regclass);


--
--

ALTER TABLE ONLY compound_families
    ADD CONSTRAINT compound_families_pkey PRIMARY KEY (parent_id, child_id);


--
--

ALTER TABLE ONLY compound_list
    ADD CONSTRAINT compound_list_id_key UNIQUE (id);


--
--

ALTER TABLE ONLY compound_list
    ADD CONSTRAINT compound_list_pkey PRIMARY KEY (name);


--
--

ALTER TABLE ONLY compound_uids
    ADD CONSTRAINT compound_uids_pkey PRIMARY KEY (id, uid);


--
--

ALTER TABLE ONLY daemon_heartbeat
    ADD CONSTRAINT daemon_heartbeat_daemonid_key UNIQUE (daemonid);


--
--

ALTER TABLE ONLY daemon_list
    ADD CONSTRAINT daemon_list_daemonid_key UNIQUE (daemonid);


--
--

ALTER TABLE ONLY daemon_list
    ADD CONSTRAINT daemon_list_pkey PRIMARY KEY (description);


--
--

ALTER TABLE ONLY setvalue_requests
    ADD CONSTRAINT setvalue_requests_pkey PRIMARY KEY (id);


--
--

ALTER TABLE ONLY state_types
    ADD CONSTRAINT state_types_pkey PRIMARY KEY (typename);


--
--

ALTER TABLE ONLY state_types
    ADD CONSTRAINT state_types_type_key UNIQUE (type);


--
--

ALTER TABLE ONLY uid_configs
    ADD CONSTRAINT uid_configs_pkey PRIMARY KEY (uid, name);


--
--

ALTER TABLE ONLY uid_daemon_connection
    ADD CONSTRAINT uid_daemon_connection_uid_key UNIQUE (uid);


--
--

ALTER TABLE ONLY uid_list
    ADD CONSTRAINT uid_list_pkey PRIMARY KEY (description);


--
--

ALTER TABLE ONLY uid_list
    ADD CONSTRAINT uid_list_uid_key UNIQUE (uid);


--
--

ALTER TABLE ONLY uid_states
    ADD CONSTRAINT uid_states_pkey PRIMARY KEY (uid);


--
--

CREATE INDEX measurements_bool_uid_time_idx ON measurements_bool USING btree (uid, "time");


--
--

CREATE INDEX measurements_float_uid_time_idx ON measurements_float USING btree (uid, "time");


--
--

CREATE INDEX measurements_int2_uid_time_idx ON measurements_int2 USING btree (uid, "time");


--
--

CREATE INDEX measurements_int4_uid_time_idx ON measurements_int4 USING btree (uid, "time");


--
--

CREATE INDEX measurements_int8_uid_time_idx ON measurements_int8 USING btree (uid, "time");


--
--

CREATE INDEX measurements_trigger_uid_time_idx ON measurements_trigger USING btree (uid, "time");


--
--

CREATE INDEX setvalue_requests_uid_index ON setvalue_requests USING btree (uid);


--
--

CREATE INDEX uid_daemon_connection_daemonid_index ON uid_daemon_connection USING btree (daemonid);


--
--

CREATE RULE daemon_list_to_heartbeart AS
    ON INSERT TO daemon_list DO  INSERT INTO daemon_heartbeat (daemonid)
  VALUES (new.daemonid);


--
--

CREATE RULE setvalue_resquest_notify AS
    ON INSERT TO setvalue_requests DO
 NOTIFY setvalue_request;


--
--

CREATE RULE setvalue_update_notify AS
    ON UPDATE TO setvalue_requests DO
 NOTIFY setvalue_update;


--
--

CREATE RULE uid_config_history_saver AS
    ON UPDATE TO uid_configs DO  INSERT INTO uid_config_history (uid, name, value, comment, valid_from)
  VALUES (old.uid, old.name, old.value, old.comment, old.last_change);


--
--

CREATE RULE uid_config_notify AS
    ON UPDATE TO uid_configs DO
 NOTIFY uid_configs_update;


--
--

CREATE RULE uid_state_history_saver AS
    ON UPDATE TO uid_states DO  INSERT INTO uid_state_history (uid, type, valid_from, valid_to, reason)
  VALUES (old.uid, old.type, old.valid_from, new.valid_from, old.reason);


--
--

ALTER TABLE ONLY compound_families
    ADD CONSTRAINT compound_families_child_id_fkey FOREIGN KEY (child_id) REFERENCES compound_list(id);


--
--

ALTER TABLE ONLY compound_families
    ADD CONSTRAINT compound_families_parent_id_fkey FOREIGN KEY (parent_id) REFERENCES compound_list(id);


--
--

ALTER TABLE ONLY compound_uids
    ADD CONSTRAINT compound_uids_id_fkey FOREIGN KEY (id) REFERENCES compound_list(id);


--
--

ALTER TABLE ONLY compound_uids
    ADD CONSTRAINT compound_uids_uid_fkey FOREIGN KEY (uid) REFERENCES uid_list(uid);


--
--

ALTER TABLE ONLY setvalue_requests
    ADD CONSTRAINT setvalue_requests_uid_fkey FOREIGN KEY (uid) REFERENCES uid_list(uid);


--
--

ALTER TABLE ONLY uid_daemon_connection
    ADD CONSTRAINT uid_daemon_connection_daemonid_fkey FOREIGN KEY (daemonid) REFERENCES daemon_list(daemonid);


--
--

ALTER TABLE ONLY uid_daemon_connection
    ADD CONSTRAINT uid_daemon_connection_uid_fkey FOREIGN KEY (uid) REFERENCES uid_list(uid);


--
--

ALTER TABLE ONLY uid_state_history
    ADD CONSTRAINT uid_state_history_type_fkey FOREIGN KEY (type) REFERENCES state_types(type);


--
--

ALTER TABLE ONLY uid_state_history
    ADD CONSTRAINT uid_state_history_uid_fkey FOREIGN KEY (uid) REFERENCES uid_list(uid);


--
--

ALTER TABLE ONLY uid_states
    ADD CONSTRAINT uid_states_type_fkey FOREIGN KEY (type) REFERENCES state_types(type);


--
--

ALTER TABLE ONLY uid_states
    ADD CONSTRAINT uid_states_uid_fkey FOREIGN KEY (uid) REFERENCES uid_list(uid);


--
-- Name: public; Type: ACL; Schema: -; Owner: postgres
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM postgres;
GRANT ALL ON SCHEMA public TO postgres;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- PostgreSQL database dump complete
--

