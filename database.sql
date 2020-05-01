--
-- PostgreSQL database dump
--

-- Dumped from database version 11.5 (Raspbian 11.5-1+deb10u1)
-- Dumped by pg_dump version 12.2

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

--
--

CREATE TABLE public.comments (
    uid integer,
    comment text,
    "time" timestamp with time zone DEFAULT now()
);



--
--

CREATE TABLE public.compound_families (
    parent_id integer NOT NULL,
    child_id integer NOT NULL,
    child_name text
);



--
--

CREATE TABLE public.compound_list (
    name text NOT NULL,
    description text,
    id integer NOT NULL
);



--
--

CREATE SEQUENCE public.compound_list_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE public.compound_list_id_seq OWNED BY public.compound_list.id;


--
--

CREATE TABLE public.compound_uids (
    id integer NOT NULL,
    uid integer NOT NULL,
    child_name text
);



--
--

CREATE TABLE public.daemon_heartbeat (
    daemonid integer,
    daemon_time timestamp with time zone,
    server_time timestamp with time zone DEFAULT now(),
    next_beat timestamp with time zone
)
WITH (fillfactor='10');



--
--

CREATE TABLE public.daemon_list (
    daemonid integer NOT NULL,
    description text NOT NULL,
    name text,
    host text
);



--
--

CREATE SEQUENCE public.daemon_list_daemonid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE public.daemon_list_daemonid_seq OWNED BY public.daemon_list.daemonid;


--
--

CREATE TABLE public.latest_measurements_bool (
    uid integer NOT NULL,
    "time" timestamp with time zone,
    value boolean
);



--
--

CREATE TABLE public.latest_measurements_float (
    uid integer NOT NULL,
    "time" timestamp with time zone,
    value real
);



--
--

CREATE TABLE public.latest_measurements_int2 (
    uid integer NOT NULL,
    "time" timestamp with time zone,
    value smallint
);



--
--

CREATE TABLE public.latest_measurements_int4 (
    uid integer NOT NULL,
    "time" timestamp with time zone,
    value integer
);



--
--

CREATE TABLE public.latest_measurements_int8 (
    uid integer NOT NULL,
    "time" timestamp with time zone,
    value bigint
);



--
--

CREATE TABLE public.latest_measurements_trigger (
    uid integer NOT NULL,
    "time" timestamp with time zone
);



--
--

CREATE TABLE public.measurements_bool (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value boolean
);



--
--

CREATE TABLE public.measurements_float (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value real NOT NULL
);



--
--

CREATE TABLE public.measurements_float_old (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value real NOT NULL
);



--
--

CREATE TABLE public.measurements_int2 (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value smallint
);



--
--

CREATE TABLE public.measurements_int4 (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value integer
);



--
--

CREATE TABLE public.measurements_int8 (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL,
    value bigint
);



--
--

CREATE TABLE public.measurements_trigger (
    uid integer NOT NULL,
    "time" timestamp with time zone NOT NULL
);



--
--

CREATE TABLE public.node_types (
    type text NOT NULL,
    nparents integer,
    comment text,
    typeid integer NOT NULL
);



--
--

CREATE SEQUENCE public.node_types_typeid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE public.node_types_typeid_seq OWNED BY public.node_types.typeid;


--
--

CREATE TABLE public.rule_config_history (
    nodeid integer,
    name text,
    value text,
    comment text,
    valid_from timestamp with time zone,
    valid_to timestamp with time zone DEFAULT now()
);



--
--

CREATE TABLE public.rule_configs (
    nodeid integer NOT NULL,
    name text NOT NULL,
    value text,
    comment text,
    last_change timestamp with time zone DEFAULT now()
);



--
--

CREATE TABLE public.rule_node_parents (
    nodeid integer NOT NULL,
    parent integer NOT NULL,
    slot text
);



--
--

CREATE TABLE public.rule_nodes (
    nodetype text NOT NULL,
    nodename text NOT NULL,
    nodeid integer NOT NULL
);



--
--

CREATE SEQUENCE public.rule_nodes_nodeid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE public.rule_nodes_nodeid_seq OWNED BY public.rule_nodes.nodeid;


--
--

CREATE TABLE public.setvalue_requests (
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

CREATE SEQUENCE public.setvalue_requests_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE public.setvalue_requests_id_seq OWNED BY public.setvalue_requests.id;


--
--

CREATE TABLE public.site_links (
    url text,
    name text,
    context text,
    number integer
);



--
--

CREATE TABLE public.state_types (
    typename text NOT NULL,
    explanation text,
    type integer NOT NULL,
    class text
);



--
--

CREATE SEQUENCE public.state_types_type_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE public.state_types_type_seq OWNED BY public.state_types.type;


--
--

CREATE TABLE public.test (
    bla integer,
    bli text
);



--
--

CREATE TABLE public.time_intervals (
    tdiff interval,
    name text
);



--
--

CREATE TABLE public.uid_config_history (
    uid integer,
    name text,
    value text,
    comment text,
    valid_from timestamp with time zone,
    valid_to timestamp with time zone DEFAULT now()
);



--
--

CREATE TABLE public.uid_configs (
    uid integer NOT NULL,
    name text NOT NULL,
    value text,
    comment text,
    last_change timestamp with time zone DEFAULT now()
);



--
--

CREATE TABLE public.uid_daemon_connection (
    uid integer,
    daemonid integer
);



--
--

CREATE TABLE public.uid_list (
    description text NOT NULL,
    uid integer NOT NULL,
    data_table text,
    is_write_value boolean DEFAULT false
);



--
--

CREATE SEQUENCE public.uid_list_uid_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;



--
--

ALTER SEQUENCE public.uid_list_uid_seq OWNED BY public.uid_list.uid;


--
--

CREATE TABLE public.uid_state_history (
    uid integer,
    type integer,
    valid_from timestamp with time zone,
    valid_to timestamp with time zone,
    reason text
);



--
--

CREATE TABLE public.uid_states (
    uid integer NOT NULL,
    type integer,
    valid_from timestamp with time zone,
    reason text
);



--
--

ALTER TABLE ONLY public.compound_list ALTER COLUMN id SET DEFAULT nextval('public.compound_list_id_seq'::regclass);


--
--

ALTER TABLE ONLY public.daemon_list ALTER COLUMN daemonid SET DEFAULT nextval('public.daemon_list_daemonid_seq'::regclass);


--
--

ALTER TABLE ONLY public.node_types ALTER COLUMN typeid SET DEFAULT nextval('public.node_types_typeid_seq'::regclass);


--
--

ALTER TABLE ONLY public.rule_nodes ALTER COLUMN nodeid SET DEFAULT nextval('public.rule_nodes_nodeid_seq'::regclass);


--
--

ALTER TABLE ONLY public.setvalue_requests ALTER COLUMN id SET DEFAULT nextval('public.setvalue_requests_id_seq'::regclass);


--
--

ALTER TABLE ONLY public.state_types ALTER COLUMN type SET DEFAULT nextval('public.state_types_type_seq'::regclass);


--
--

ALTER TABLE ONLY public.uid_list ALTER COLUMN uid SET DEFAULT nextval('public.uid_list_uid_seq'::regclass);


--
--

ALTER TABLE ONLY public.compound_families
    ADD CONSTRAINT compound_families_pkey PRIMARY KEY (parent_id, child_id);


--
--

ALTER TABLE ONLY public.compound_list
    ADD CONSTRAINT compound_list_id_key UNIQUE (id);


--
--

ALTER TABLE ONLY public.compound_list
    ADD CONSTRAINT compound_list_pkey PRIMARY KEY (name);


--
--

ALTER TABLE ONLY public.compound_uids
    ADD CONSTRAINT compound_uids_pkey PRIMARY KEY (id, uid);


--
--

ALTER TABLE ONLY public.daemon_heartbeat
    ADD CONSTRAINT daemon_heartbeat_daemonid_key UNIQUE (daemonid);


--
--

ALTER TABLE ONLY public.daemon_list
    ADD CONSTRAINT daemon_list_daemonid_key UNIQUE (daemonid);


--
--

ALTER TABLE ONLY public.daemon_list
    ADD CONSTRAINT daemon_list_pkey PRIMARY KEY (description);


--
--

ALTER TABLE ONLY public.latest_measurements_bool
    ADD CONSTRAINT latest_measurements_bool_pkey PRIMARY KEY (uid);


--
--

ALTER TABLE ONLY public.latest_measurements_float
    ADD CONSTRAINT latest_measurements_float_pkey PRIMARY KEY (uid);


--
--

ALTER TABLE ONLY public.latest_measurements_int2
    ADD CONSTRAINT latest_measurements_int2_pkey PRIMARY KEY (uid);


--
--

ALTER TABLE ONLY public.latest_measurements_int4
    ADD CONSTRAINT latest_measurements_int4_pkey PRIMARY KEY (uid);


--
--

ALTER TABLE ONLY public.latest_measurements_int8
    ADD CONSTRAINT latest_measurements_int8_pkey PRIMARY KEY (uid);


--
--

ALTER TABLE ONLY public.latest_measurements_trigger
    ADD CONSTRAINT latest_measurements_trigger_pkey PRIMARY KEY (uid);


--
--

ALTER TABLE ONLY public.node_types
    ADD CONSTRAINT node_types_pkey PRIMARY KEY (type);


--
--

ALTER TABLE ONLY public.rule_configs
    ADD CONSTRAINT rule_configs_pkey PRIMARY KEY (nodeid, name);


--
--

ALTER TABLE ONLY public.rule_node_parents
    ADD CONSTRAINT rule_node_parents_pkey PRIMARY KEY (nodeid, parent);


--
--

ALTER TABLE ONLY public.rule_nodes
    ADD CONSTRAINT rule_nodes_pkey PRIMARY KEY (nodetype, nodename);


--
--

ALTER TABLE ONLY public.setvalue_requests
    ADD CONSTRAINT setvalue_requests_pkey PRIMARY KEY (id);


--
--

ALTER TABLE ONLY public.state_types
    ADD CONSTRAINT state_types_pkey PRIMARY KEY (typename);


--
--

ALTER TABLE ONLY public.state_types
    ADD CONSTRAINT state_types_type_key UNIQUE (type);


--
--

ALTER TABLE ONLY public.uid_configs
    ADD CONSTRAINT uid_configs_pkey PRIMARY KEY (uid, name);


--
--

ALTER TABLE ONLY public.uid_daemon_connection
    ADD CONSTRAINT uid_daemon_connection_uid_key UNIQUE (uid);


--
--

ALTER TABLE ONLY public.uid_list
    ADD CONSTRAINT uid_list_pkey PRIMARY KEY (description);


--
--

ALTER TABLE ONLY public.uid_list
    ADD CONSTRAINT uid_list_uid_key UNIQUE (uid);


--
--

ALTER TABLE ONLY public.uid_states
    ADD CONSTRAINT uid_states_pkey PRIMARY KEY (uid);


--
--

CREATE INDEX measurements_bool_uid_time_idx ON public.measurements_bool USING btree (uid, "time");


--
--

CREATE INDEX measurements_float_uid_time_idx ON public.measurements_float USING btree (uid, "time");


--
--

CREATE INDEX measurements_int2_uid_time_idx ON public.measurements_int2 USING btree (uid, "time");


--
--

CREATE INDEX measurements_int4_uid_time_idx ON public.measurements_int4 USING btree (uid, "time");


--
--

CREATE INDEX measurements_int8_uid_time_idx ON public.measurements_int8 USING btree (uid, "time");


--
--

CREATE INDEX measurements_trigger_uid_time_idx ON public.measurements_trigger USING btree (uid, "time");


--
--

CREATE INDEX setvalue_requests_uid_index ON public.setvalue_requests USING btree (uid);


--
--

CREATE INDEX uid_daemon_connection_daemonid_index ON public.uid_daemon_connection USING btree (daemonid);


--
--

CREATE RULE daemon_list_to_heartbeart AS
    ON INSERT TO public.daemon_list DO  INSERT INTO public.daemon_heartbeat (daemonid)
  VALUES (new.daemonid);


--
--

CREATE RULE lastest_measurements_bool_updater AS
    ON INSERT TO public.measurements_bool DO  UPDATE public.latest_measurements_bool SET "time" = new."time", value = new.value
  WHERE (latest_measurements_bool.uid = new.uid);


--
--

CREATE RULE lastest_measurements_float_updater AS
    ON INSERT TO public.measurements_float DO  UPDATE public.latest_measurements_float SET "time" = new."time", value = new.value
  WHERE (latest_measurements_float.uid = new.uid);


--
--

CREATE RULE lastest_measurements_int2_updater AS
    ON INSERT TO public.measurements_int2 DO  UPDATE public.latest_measurements_int2 SET "time" = new."time", value = new.value
  WHERE (latest_measurements_int2.uid = new.uid);


--
--

CREATE RULE lastest_measurements_int4_updater AS
    ON INSERT TO public.measurements_int4 DO  UPDATE public.latest_measurements_int4 SET "time" = new."time", value = new.value
  WHERE (latest_measurements_int4.uid = new.uid);


--
--

CREATE RULE lastest_measurements_int8_updater AS
    ON INSERT TO public.measurements_int8 DO  UPDATE public.latest_measurements_int8 SET "time" = new."time", value = new.value
  WHERE (latest_measurements_int8.uid = new.uid);


--
--

CREATE RULE lastest_measurements_trigger_updater AS
    ON INSERT TO public.measurements_trigger DO  UPDATE public.latest_measurements_trigger SET "time" = new."time"
  WHERE (latest_measurements_trigger.uid = new.uid);


--
--

CREATE RULE rule_config_history_saver AS
    ON UPDATE TO public.rule_configs DO  INSERT INTO public.rule_config_history (nodeid, name, value, comment, valid_from)
  VALUES (old.nodeid, old.name, old.value, old.comment, old.last_change);


--
--

CREATE RULE ruleprocessornotify AS
    ON INSERT TO public.measurements_trigger DO  SELECT pg_notify('ruleprocessor_measurements_trigger'::text, (new.uid)::text) AS pg_notify;


--
--

CREATE RULE ruleprocessornotify AS
    ON INSERT TO public.measurements_float DO  SELECT pg_notify('ruleprocessor_measurements_float'::text, (new.uid)::text) AS pg_notify
   FROM (public.uid_list
     JOIN public.rule_nodes ON ((uid_list.description = rule_nodes.nodename)))
  WHERE ((rule_nodes.nodetype = 'measurement'::text) AND (uid_list.uid = new.uid));


--
--

CREATE RULE ruleprocessornotify AS
    ON INSERT TO public.measurements_bool DO  SELECT pg_notify('ruleprocessor_measurements_bool'::text, (new.uid)::text) AS pg_notify;


--
--

CREATE RULE setvalue_request_notify AS
    ON INSERT TO public.setvalue_requests DO  SELECT pg_notify('setvalue_request'::text, (new.uid)::text) AS pg_notify;


--
--

CREATE RULE setvalue_request_specific AS
    ON UPDATE TO public.setvalue_requests DO  SELECT pg_notify(('setvalue_request_'::text || (uid_daemon_connection.daemonid)::text), (new.uid)::text) AS pg_notify
   FROM public.uid_daemon_connection
  WHERE (uid_daemon_connection.uid = new.uid);


--
--

CREATE RULE setvalue_update_notify AS
    ON UPDATE TO public.setvalue_requests DO  SELECT pg_notify('setvalue_update'::text, (new.uid)::text) AS pg_notify;


--
--

CREATE RULE testnotify AS
    ON INSERT TO public.test DO  SELECT pg_notify('mist'::text, ((new.bla)::text || new.bli)) AS pg_notify;


--
--

CREATE RULE uid_config_history_saver AS
    ON UPDATE TO public.uid_configs DO  INSERT INTO public.uid_config_history (uid, name, value, comment, valid_from)
  VALUES (old.uid, old.name, old.value, old.comment, old.last_change);


--
--

CREATE RULE uid_config_notify AS
    ON UPDATE TO public.uid_configs DO  SELECT pg_notify('uid_configs_update'::text, (new.uid)::text) AS pg_notify;


--
--

CREATE RULE uid_config_notify_spcific AS
    ON UPDATE TO public.uid_configs DO  SELECT pg_notify(('uid_configs_update_'::text || (uid_daemon_connection.daemonid)::text), (new.uid)::text) AS pg_notify
   FROM public.uid_daemon_connection
  WHERE (uid_daemon_connection.uid = new.uid);


--
--

CREATE RULE uid_state_history_saver AS
    ON UPDATE TO public.uid_states DO  INSERT INTO public.uid_state_history (uid, type, valid_from, valid_to, reason)
  VALUES (old.uid, old.type, old.valid_from, new.valid_from, old.reason);


--
--

ALTER TABLE ONLY public.compound_families
    ADD CONSTRAINT compound_families_child_id_fkey FOREIGN KEY (child_id) REFERENCES public.compound_list(id);


--
--

ALTER TABLE ONLY public.compound_families
    ADD CONSTRAINT compound_families_parent_id_fkey FOREIGN KEY (parent_id) REFERENCES public.compound_list(id);


--
--

ALTER TABLE ONLY public.compound_uids
    ADD CONSTRAINT compound_uids_id_fkey FOREIGN KEY (id) REFERENCES public.compound_list(id);


--
--

ALTER TABLE ONLY public.compound_uids
    ADD CONSTRAINT compound_uids_uid_fkey FOREIGN KEY (uid) REFERENCES public.uid_list(uid);


--
--

ALTER TABLE ONLY public.setvalue_requests
    ADD CONSTRAINT setvalue_requests_uid_fkey FOREIGN KEY (uid) REFERENCES public.uid_list(uid);


--
--

ALTER TABLE ONLY public.uid_daemon_connection
    ADD CONSTRAINT uid_daemon_connection_daemonid_fkey FOREIGN KEY (daemonid) REFERENCES public.daemon_list(daemonid);


--
--

ALTER TABLE ONLY public.uid_daemon_connection
    ADD CONSTRAINT uid_daemon_connection_uid_fkey FOREIGN KEY (uid) REFERENCES public.uid_list(uid);


--
--

ALTER TABLE ONLY public.uid_state_history
    ADD CONSTRAINT uid_state_history_type_fkey FOREIGN KEY (type) REFERENCES public.state_types(type);


--
--

ALTER TABLE ONLY public.uid_state_history
    ADD CONSTRAINT uid_state_history_uid_fkey FOREIGN KEY (uid) REFERENCES public.uid_list(uid);


--
--

ALTER TABLE ONLY public.uid_states
    ADD CONSTRAINT uid_states_type_fkey FOREIGN KEY (type) REFERENCES public.state_types(type);


--
--

ALTER TABLE ONLY public.uid_states
    ADD CONSTRAINT uid_states_uid_fkey FOREIGN KEY (uid) REFERENCES public.uid_list(uid);


--
-- PostgreSQL database dump complete
--

