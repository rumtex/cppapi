
INSERT INTO periods (begin_date, end_date) VALUES
	('2020-02-01', '2020-03-31');

INSERT INTO departments (name) VALUES
	('СМЭВ'),
	('Министерство экономического развития Саратовской области');

INSERT INTO users (username, password, department_id, role, created_at, updated_at) VALUES
	('admin', encode(digest(concat('123qweASD', :'salt'), 'sha1'), 'hex'), 2, 'администратор', '2020-01-01T00:00:00', '2020-01-01T00:00:00'),
	('ЕПГУ', '0', 1, 'оператор', '2020-01-01T00:00:00', '2020-01-01T00:00:00');

INSERT INTO preference_types (name, priority_index, comment) VALUES
	('Внеочередное право', 1, '2.8.1. Внеочередное право предоставления сертификатов в лагеря установлено для детей прокуроров, сотрудников Следственного комитета Российской Федерации, судей.'),
	('Первоочередное право', 2, '2.8.2. Первоочередное право предоставления сертификатов в лагеря установлено для детей граждан, указанных в ФЗ "О дополнительных гарантиях по социальной поддержке детей-сирот и детей, оставшихся без попечения родителей", ФЗ "О социальных гарантиях сотрудникам некоторых федеральных органов исполнительной власти и внесении изменений в отдельные законодательные акты Российской Федерации", ФЗ "О полиции", ФЗ "О статусе военнослужащих", ЗСО "О мерах социальной поддержки многодетных семей в Саратовской области".');

