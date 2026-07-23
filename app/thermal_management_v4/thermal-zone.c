struct tz_dt_spec {
	const struct device *sensor;
	const int *trips;
	const int *levels;
	const struct device *cooling_devices;
}

#define TZ_INIT(inst)				\
	const int trips_##inst##[DT_PROP_LEN(inst, trips)] = DT_PROP(inst, trips);	\
											\
	const int levels_##inst##[DT_PROP_LEN(inst, levels)] = DT_PROP(inst, levels);	\
											\
	static const struct tz_config tz_##inst##_config = {	\
		.trips = &trips_##inst##,			\
		.levels = &levels_##inst##,			\
		.sensor = DEVICE_DT_GET(DT_PHANDLE(inst, sensor)),	\
	};
		
DT_INST_FOREACH_STATUS_OKAY(TZ_INIT)
