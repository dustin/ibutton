-- Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
--
-- $Id: sensors.sql,v 1.2 2002/01/26 23:23:55 dustin Exp $
-- arch-tag: CDF89BEC-13E5-11D8-B988-000393CFE6B8

-- Different types of sensors
create table sensor_types (
	sensor_type_id serial,
	sensor_type text not null,
	units varchar(10) not null,
	primary key(sensor_type_id)
);
grant select on sensor_types to nobody;

-- Add a sensor
insert into sensor_types(sensor_type, units) values('Thermometer', 'deg C');

-- Here's a description of the sensors.
create table sensors (
	sensor_id serial,
	sensor_type_id integer not null,
	serial char(16) not null,
	name text not null,
	low smallint not null,
	high smallint not null,
	active boolean default true,
	primary key(sensor_id),
	foreign key(sensor_type_id) references sensor_types(sensor_type_id)
);
create unique index sensors_byserial on sensors(serial);
grant select on sensors to tempload;
grant select on sensors to nobody;

-- The actual samples go here.
create table samples (
	ts datetime not null,
	sensor_id integer not null,
	sample float not null,
	foreign key(sensor_id) references sensors(sensor_id)
);
create index samples_bytime on samples(ts);
create index samples_byid on samples(sensor_id);
create unique index samples_bytimeid on samples(ts, sensor_id);
grant insert on samples to tempload;
grant select on samples to nobody;

create table rollups (
	sensor_id integer not null,
	min_reading float not null,
	avg_reading float not null,
	stddev_reading float not null,
	max_reading float not null,
	month date not null,
	foreign key(sensor_id) references sensors(sensor_id)
);
create unique index rollups_senstype on rollups(sensor_id, month);
grant select on rollups to nobody;

-- Migration
insert into rollups (sensor_id, month, min_reading, avg_reading,
		stddev_reading, max_reading)
	select sensor_id, date_trunc('mon', ts) as mon,
		min(sample), avg(sample), stddev(sample), max(sample)
	from samples
		where ts > '12/1/2003'
	group by sensor_id, mon
;

-- XXX:  Need to create a trigger to maintain the rollups table on insert to
-- the samples table.

-- where ts between (date_trunc('mon', now()))
--		and (date_trunc('mon', now()) + '1 month'::reltime)

-- View of the data
create view sample_view as
	select samples.ts, sensors.sensor_id, sensors.serial,
			sensors.name, samples.sample as c,
			(samples.sample*9/5)+32 as f
		from samples, sensors
		where
			samples.sensor_id = sensors.sensor_id
;
grant select on sample_view to nobody;
