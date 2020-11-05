CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

CREATE TABLE departments
(
    id serial PRIMARY KEY NOT NULL,
    name varchar(120) UNIQUE NOT NULL,
    removed boolean NOT NULL DEFAULT false
);

CREATE TYPE user_role AS ENUM (
    'администратор',
    'ответственный',
    'оператор'
);

CREATE TABLE users
(
    id serial PRIMARY KEY NOT NULL,
    username varchar(20) UNIQUE NOT NULL,
    password varchar(40) NOT NULL,
    role user_role NOT NULL,
    family_name varchar(100),
    first_name varchar(100),
    patronimic varchar(100),
    staff varchar(100),
    email varchar(100),
    phone varchar(12),
    department_id int NOT NULL, FOREIGN KEY (department_id) REFERENCES departments(id),
    tries integer NOT NULL DEFAULT 3,
    last_try timestamp without time zone,
    created_at timestamp without time zone NOT NULL,
    updated_at timestamp without time zone NOT NULL,
    removed boolean NOT NULL DEFAULT false
);

CREATE TABLE sessions
(
    id serial PRIMARY KEY NOT NULL,
    useragent varchar(150),
    ip_address varchar(16),
    active boolean DEFAULT true NOT NULL,
    user_id int NOT NULL, FOREIGN KEY (user_id) REFERENCES users(id),
    created_at timestamp without time zone NOT NULL
);

CREATE TABLE session_keys
(
    id uuid PRIMARY KEY DEFAULT uuid_generate_v4() NOT NULL,
    session_id integer NOT NULL, FOREIGN KEY (session_id) REFERENCES sessions(id),
    created_at timestamp without time zone NOT NULL
);

CREATE TABLE actions_history
(
    id serial PRIMARY KEY,
    action varchar(240) NOT NULL,
    session_id integer, FOREIGN KEY (session_id) REFERENCES sessions(id),
    created_at timestamp without time zone NOT NULL
);

CREATE TABLE addresses
(
    id serial PRIMARY KEY,
    city varchar(100) NOT NULL,
    street varchar(100) NOT NULL,
    house varchar(10) NOT NULL,
    apartment varchar(10) NOT NULL
);

CREATE TABLE periods
(
    id serial PRIMARY KEY,
    begin_date date NOT NULL,
    end_date date NOT NULL,
    removed boolean NOT NULL DEFAULT false
);

CREATE TABLE period_assign_results
(
    id serial PRIMARY KEY,
    period_id integer, FOREIGN KEY (period_id) REFERENCES periods(id),
    data text NOT NULL,
    created_at timestamp without time zone NOT NULL
);

CREATE TABLE contact_info
(
    id serial PRIMARY KEY,
    mobile_phone varchar(12) NOT NULL,
    home_phone varchar(12),
    work_phone varchar(12),
    email varchar(100) NOT NULL
);

CREATE TYPE gender_type AS ENUM (
    'мужской',
    'женский'
);

CREATE TABLE applicants
(
    id serial PRIMARY KEY,
    family_name varchar(100) NOT NULL,
    first_name varchar(100) NOT NULL,
    patronimic varchar(100) NULL,
    gender gender_type,
    address_id integer NOT NULL, FOREIGN KEY (address_id) REFERENCES addresses(id),
    contact_info_id integer NOT NULL, FOREIGN KEY (contact_info_id) REFERENCES contact_info(id)
);

CREATE INDEX index_applicants_on_first_name ON applicants USING btree(first_name);
CREATE INDEX index_applicants_on_family_name ON applicants USING btree(family_name);
CREATE INDEX index_applicants_on_patronimic ON applicants USING btree(patronimic);

CREATE TABLE work_places
(
    id serial PRIMARY KEY,
    applicant_id integer NOT NULL, FOREIGN KEY (applicant_id) REFERENCES applicants(id),
    work_place varchar(100),
    work_seat varchar(100)
);

CREATE TABLE administrative_parts
(
    id serial PRIMARY KEY,
    name varchar(100) NOT NULL,
    removed boolean NOT NULL DEFAULT false
);

CREATE UNIQUE INDEX index_administrative_parts_on_name ON administrative_parts USING btree(name);

CREATE TABLE schools
(
    id serial PRIMARY KEY,
    administrative_part_id integer NOT NULL, FOREIGN KEY (administrative_part_id) REFERENCES administrative_parts(id),
    name varchar(200) NOT NULL,
    removed boolean NOT NULL DEFAULT false
);

CREATE UNIQUE INDEX index_schools_on_name ON schools USING btree(name);

CREATE TABLE childs
(
    id serial PRIMARY KEY,
    family_name varchar(100) NOT NULL,
    first_name varchar(100) NOT NULL,
    patronimic varchar(100),
    gender gender_type,
    birth_date date NOT NULL,
    birth_place varchar(200) NOT NULL,
    address_id integer NOT NULL, FOREIGN KEY (address_id) REFERENCES addresses(id)
);

CREATE INDEX index_childs_on_first_name ON childs USING btree(first_name);
CREATE INDEX index_childs_on_family_name ON childs USING btree(family_name);
CREATE INDEX index_childs_on_patronimic ON childs USING btree(patronimic);

CREATE TYPE application_status_type AS ENUM (
    'в работе',
    'отказано',
    'в очереди',
    'Подтвердить вручную!',
    -- 'распределен',
    'заявитель уведомлен',
    'услуга оказана',
    'не распределено'
);

CREATE TYPE relation_type AS ENUM (
    'мать',
    'отец',
    'опекун',
    'гендальф'
);

CREATE TYPE preferred_inform_type AS ENUM (
    'телефон',
    'почта',
    'электронная почта'
);

CREATE TABLE applications
(
    id serial PRIMARY KEY,
    epgu_id bigint,
    status application_status_type NOT NULL DEFAULT 'в работе',
    applicant_id integer NOT NULL, FOREIGN KEY (applicant_id) REFERENCES applicants(id),
    child_id integer NOT NULL, FOREIGN KEY (child_id) REFERENCES childs(id),
    relation_with relation_type NOT NULL,
    preferred_inform preferred_inform_type NOT NULL,
    has_preference boolean NOT NULL,
    other_camps boolean NOT NULL,
    creator_id integer NOT NULL, FOREIGN KEY (creator_id) REFERENCES users(id),
    updated_at timestamp without time zone NOT NULL,
    created_at timestamp without time zone NOT NULL
);

CREATE UNIQUE INDEX index_applications_on_epgu_id ON applications USING btree(epgu_id);

CREATE TABLE camps
(
    id serial PRIMARY KEY,
    name varchar(100) NOT NULL,
    is_sport boolean DEFAULT false NOT NULL,
    removed boolean NOT NULL DEFAULT false
);

CREATE UNIQUE INDEX index_camps_on_name ON camps USING btree(name);

CREATE TABLE camp_periods
(
    id serial PRIMARY KEY,
    camp_id integer NOT NULL, FOREIGN KEY (camp_id) REFERENCES camps(id),
    name varchar(100),
    begin_date date NOT NULL,
    end_date date NOT NULL,
    count bigint NOT NULL DEFAULT 0,
    removed boolean NOT NULL DEFAULT false
);

CREATE INDEX index_camp_periods_on_name ON camp_periods USING btree(name);

CREATE TABLE choised_camp_periods
(
    id serial PRIMARY KEY,
    camp_id integer NOT NULL, FOREIGN KEY (camp_id) REFERENCES camps(id),
    camp_period_id integer, FOREIGN KEY (camp_period_id) REFERENCES camp_periods(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id),
    priority_index integer NOT NULL
);

CREATE TABLE reserved_certificates
(
    id serial PRIMARY KEY,
    camp_period_id integer NOT NULL, FOREIGN KEY (camp_period_id) REFERENCES camp_periods(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id),
    updated_at timestamp without time zone NOT NULL,
    created_at timestamp without time zone NOT NULL
);

CREATE TABLE issued_certificates
(
    id serial PRIMARY KEY,
    num varchar(11) NOT NULL,
    camp_period_id integer NOT NULL, FOREIGN KEY (camp_period_id) REFERENCES camp_periods(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id),
    created_at timestamp without time zone NOT NULL
);

CREATE TABLE files
(
    id serial PRIMARY KEY,
    file bytea NOT NULL,
    mime varchar(200) NOT NULL,
    size int NOT NULL,
    created_at timestamp without time zone NOT NULL
);

CREATE TYPE doc_type AS ENUM (
    'справка из органов опеки',
    'справка из МОУ',
    'справка из спортивной школы',
    'подтверждающий льготу',
    'удостоверяющий личность',
    'вид на жительство в РФ',
    'заявление'
);

CREATE TABLE documents
(
    id serial PRIMARY KEY,
    name varchar(256) NOT NULL,
    file_id integer NOT NULL, FOREIGN KEY (file_id) REFERENCES files(id),
    type doc_type NOT NULL,
    session_id integer NOT NULL, FOREIGN KEY (session_id) REFERENCES sessions(id),
    created_at timestamp without time zone NOT NULL
);

CREATE TABLE preference_types
(
    id serial PRIMARY KEY,
    priority_index integer NOT NULL,
    name varchar(100) NOT NULL,
    comment text,
    removed boolean NOT NULL DEFAULT false
);

CREATE TABLE preference_doc_types
(
    id serial PRIMARY KEY,
    name varchar(100) NOT NULL,
    comment text,
    removed boolean NOT NULL DEFAULT false
);

CREATE TABLE preference_docs
(
    id serial PRIMARY KEY,
    document_id integer NOT NULL, FOREIGN KEY (document_id) REFERENCES documents(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id),
    preference_type_id integer NOT NULL, FOREIGN KEY (preference_type_id) REFERENCES preference_types(id),
    preference_doc_type_id integer NOT NULL, FOREIGN KEY (preference_doc_type_id) REFERENCES preference_doc_types(id),
    num varchar(100) NOT NULL,
    issued_date date NOT NULL
);

CREATE TABLE school_docs
(
    id serial PRIMARY KEY,
    document_id integer NOT NULL, FOREIGN KEY (document_id) REFERENCES documents(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id),
    num varchar(100) NOT NULL,
    issued_date date NOT NULL,
    school_id integer NOT NULL, FOREIGN KEY (school_id) REFERENCES schools(id),
    class_num varchar(10) NOT NULL
);

CREATE TABLE sport_school_docs
(
    id serial PRIMARY KEY,
    document_id integer NOT NULL, FOREIGN KEY (document_id) REFERENCES documents(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id),
    num varchar(100) NOT NULL,
    issued_date date NOT NULL
);

CREATE TYPE person_doc_type AS ENUM (
    'паспорт',
    'паспорт иностранного гражданина',
    'свидетельство о рождении'
);

CREATE TABLE person_docs
(
    id serial PRIMARY KEY,
    document_id integer NOT NULL, FOREIGN KEY (document_id) REFERENCES documents(id),
    type person_doc_type NOT NULL,
    num varchar(22) NOT NULL,
    issued_date date NOT NULL,
    issuer varchar(200)
);

CREATE TABLE applicant_person_docs
(
    id serial PRIMARY KEY,
    person_doc_id integer NOT NULL, FOREIGN KEY (person_doc_id) REFERENCES person_docs(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id)
);

CREATE TABLE child_person_docs
(
    id serial PRIMARY KEY,
    person_doc_id integer NOT NULL, FOREIGN KEY (person_doc_id) REFERENCES person_docs(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id)
);

CREATE TABLE international_docs
(
    id serial PRIMARY KEY,
    document_id integer NOT NULL, FOREIGN KEY (document_id) REFERENCES documents(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id)
);

CREATE TABLE application_docs
(
    id serial PRIMARY KEY,
    document_id integer NOT NULL, FOREIGN KEY (document_id) REFERENCES documents(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id)
);

CREATE TABLE relation_docs
(
    id serial PRIMARY KEY,
    document_id integer NOT NULL, FOREIGN KEY (document_id) REFERENCES documents(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id),
    name varchar(100) NOT NULL,
    num varchar(100) NOT NULL,
    issued_date date NOT NULL
);

CREATE TABLE message_templates
(
    id serial PRIMARY KEY,
    message text NOT NULL,
    removed boolean NOT NULL DEFAULT false
);

CREATE INDEX index_message_templates_on_message ON message_templates USING btree(message);

CREATE TABLE messages
(
    id serial PRIMARY KEY,
    message_template_id integer, FOREIGN KEY (message_template_id) REFERENCES message_templates(id),
    message text
);

CREATE INDEX index_message_on_messages ON messages USING btree(message);

CREATE TYPE action_type AS ENUM (
    'принято системой',
    'обновлен статус',
    'получено сообщение',
    'отправлен ответ',
    'запрошен номер в очереди',
    'распределение'
);

CREATE TABLE applicate_history
(
    id serial PRIMARY KEY,
    type action_type NOT NULL,
    status application_status_type,
    session_id integer NOT NULL, FOREIGN KEY (session_id) REFERENCES sessions(id),
    message_id integer, FOREIGN KEY (message_id) REFERENCES messages(id),
    application_id integer NOT NULL, FOREIGN KEY (application_id) REFERENCES applications(id),
    created_at timestamp without time zone NOT NULL
);

-- индекс по двум полям
-- CREATE UNIQUE INDEX index_table_on_field_one_and_field_two ON table USING btree (field_one, field_two);
