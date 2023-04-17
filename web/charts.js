(async () => {
	const d3localeData = await new Promise(
		(resolve, reject) => 
			d3.json("https://cdn.jsdelivr.net/npm/d3-time-format@2/locale/pl-PL.json", (error, locale) => {
				if (error) {
					reject(error);
					return;
				}
				resolve(locale);
			})
	)
	d3.timeFormatDefaultLocale(d3localeData);

	////////////////////////////////////////////////////////////////////////////////

	const svg = d3.select("svg#chart");
	const boundingRect = svg.node().getBoundingClientRect();
	let totalHeight = 0;

	const padding1 = {top: 15, right: 40, bottom: 15, left: 40};
	const padding2 = {top: 15, right: 40, bottom: 30, left: 40};

	const boxHeight1 = boundingRect.height * 0.8;
	const position1 = {top: totalHeight + padding1.top, left: padding1.left};
	totalHeight += boxHeight1;
	const contentHeight1 = boxHeight1 - padding1.top - padding1.bottom;

	const boxHeight2 = boundingRect.height * 0.2;
	const position2 = {top: totalHeight + padding1.top, left: padding1.left};
	totalHeight += boxHeight2;
	const contentHeight2 = boxHeight2 - padding2.top - padding2.bottom;

	const contentWidth = boundingRect.width - padding1.right - padding1.left;

	// Time format without period (12 PM/AM etc)
	const formatTimeAxisTick = (date) => (
			  d3.timeSecond(date) < date ? d3.timeFormat(".%L")
			: d3.timeMinute(date) < date ? d3.timeFormat(":%S")
			: d3.timeHour(date)   < date ? d3.timeFormat("%H:%M")
			: d3.timeDay(date)    < date ? d3.timeFormat("%H:%M")
			: d3.timeWeek(date)   < date ? d3.timeFormat("%a %d")
			: d3.timeMonth(date)  < date ? d3.timeFormat("%b %d")
			: d3.timeYear(date)   < date ? d3.timeFormat("%B")
			: d3.timeFormat("%Y")
		)(date);

	const clip = svg.append("defs").append("svg:clipPath")
		.attr("id", "mainChartClip")
		.append("svg:rect")
		.attr("width", contentWidth)
		.attr("height", contentHeight1)
		.attr("x", 0)
		.attr("y", 0)
	;

	const main = {};
	main.gridElement = svg.append("g")
		.attr("class", "grid")
		.attr("transform", `translate(${position1.left},${position1.top})`)
	;
	main.chartElement = svg.append("g")
		.attr("class", "lines")
		.attr("transform", `translate(${position1.left},${position1.top})`)
		.attr("clip-path", "url(#mainChartClip)")
	;
	main.xScale = {};
	main.xScale.time = d3.scaleTime().range([0, contentWidth]);
	main.yScale = {};
	main.yScale.temperature = d3.scaleLinear().range([contentHeight1, 0]);
	main.yScale.phLevel = d3.scaleLinear().range([contentHeight1, 0]);
	main.xAxisGen = {};
	main.xAxisGen.time = d3.axisBottom(main.xScale.time).tickFormat(formatTimeAxisTick);;
	main.yAxisGen = {};
	main.yAxisGen.temperature = d3.axisLeft(main.yScale.temperature);
	main.yAxisGen.phLevel = d3.axisRight(main.yScale.phLevel);
	main.axesFocus = svg.append("g")
		.attr("class", "axes")
		.attr("transform", `translate(${position1.left},${position1.top})`)
	;
	main.xAxisElem = {}; 
	main.xAxisElem.time = main.axesFocus.append("g")
		.attr("class", "axis axis--x")
		.attr("transform", `translate(0,${contentHeight1})`)
	;
	main.yAxisElem = {}; 
	main.yAxisElem.temperature = main.axesFocus.append("g")
		.attr("class", "axis axis--y")
	;
	main.yAxisElem.phLevel = main.axesFocus.append("g")
		.attr("class", "axis axis--y")
		.attr("transform", `translate(${contentWidth},0)`)
	;
	main.lineGen = {};
	main.lineGen.rtcTemperature = d3.line()
		.x((sample) => main.xScale.time(sample.time))
		.y((sample) => main.yScale.temperature(sample.rtc))
	;
	main.lineGen.waterTemperature = d3.line()
		.x((sample) => main.xScale.time(sample.time))
		.y((sample) => main.yScale.temperature(sample.water))
	;
	main.lineGen.phLevel = d3.line()
		.x((sample) => main.xScale.time(sample.time))
		.y((sample) => main.yScale.phLevel(sample.ph))
	;
	main.lineElem = {};
	main.lineElem.rtcTemperature = main.chartElement.append("path").attr("class", "line line--rtcTemperature");
	main.lineElem.waterTemperature = main.chartElement.append("path").attr("class", "line line--waterTemperature");
	main.lineElem.phLevel = main.chartElement.append("path").attr("class", "line line--phLevel");

	const context = {};
	context.chartElement = svg.append("g")
		.attr("class", "context")
		.attr("transform", `translate(${position2.left},${position2.top})`)
	;
	context.xScale = {}; 
	context.xScale.time = d3.scaleTime().range([0, contentWidth]);
	context.yScale = {};
	context.yScale.temperature = d3.scaleLinear().range([contentHeight2, 0]);
	context.yScale.phLevel = d3.scaleLinear().range([contentHeight2, 0]);
	context.xAxisGen = {};
	context.xAxisGen.time = d3.axisBottom(context.xScale.time).tickFormat(formatTimeAxisTick);;
	context.xAxisElem = {};
	context.xAxisElem.time = context.chartElement.append("g")
		.attr("class", "axis axis--x")
		.attr("transform", `translate(0,${contentHeight2})`)
	;
	context.lineGen = {};
	context.lineGen.rtcTemperature = d3.line()
		.x((sample) => context.xScale.time(sample.time))
		.y((sample) => context.yScale.temperature(sample.rtc))
	;
	context.lineGen.waterTemperature = d3.line()
		.x((sample) => context.xScale.time(sample.time))
		.y((sample) => context.yScale.temperature(sample.water))
	;
	context.lineGen.phLevel = d3.line()
		.x((sample) => context.xScale.time(sample.time))
		.y((sample) => context.yScale.phLevel(sample.ph))
	;
	context.lineElem = {};
	context.lineElem.rtcTemperature = context.chartElement.append("path").attr("class", "line line--rtcTemperature");
	context.lineElem.waterTemperature = context.chartElement.append("path").attr("class", "line line--waterTemperature");
	context.lineElem.phLevel = context.chartElement.append("path").attr("class", "line line--phLevel");

	const brushElement = context.chartElement.append("g").attr("class", "brush");

	const mainAreaRect = svg.append("rect")
		.attr("class", "zoom")
		.attr("width", contentWidth)
		.attr("height", contentHeight1)
		.attr("transform", "translate(" + position1.left + "," + position1.top + ")")
	;

	const tooltipElement = main.chartElement.append("foreignObject")
		.attr("class", "tooltip")
		.attr("width", 240)
		.attr("height", 80)
	;
	const hoverLineElement = main.chartElement.append("line")
		.attr("class", "hoverLine")
	;

	const timeBisector = d3.bisector((sample) => sample.time).left;
	mainAreaRect.on('mousemove', function() {
		if (!samples || !samples[0] || !samples[0].time) return;
		const coords = d3.mouse(this);
		const hoverTime = main.xScale.time.invert(coords[0]);
		const i = timeBisector(samples, hoverTime, 1);
		const pre = samples[i - 1];
		const post = samples[i];
		const sample = hoverTime - pre.time > post.time - hoverTime ? post : pre;
		const sample_x = main.xScale.time(sample.time);

		const tooltip_x = coords[0] > contentWidth - 240 ? sample_x - 240 - 40 : sample_x + 20;

		tooltipElement
			.attr("transform", `translate(${tooltip_x},${coords[1] - 20})`)
			.style('display', 'block')
		;
		tooltipElement.selectAll("*").remove();
		tooltipElement.append("xhtml:html").html(`
			<div class="text--waterTemperature">Temperatura wody: ${sample.water.toFixed(2)}°C</div>
			<div class="text--rtcTemperature">Temperatura układu: ${sample.rtc.toFixed(2)}°C</div>
			<div class="text--phLevel">Poziom pH wody: ${sample.ph.toFixed(2)}</div>
		`); 
		hoverLineElement
			.classed('show', true)
			.attr('x1', sample_x)
			.attr('x2', sample_x)
			.attr('y1', 0)
			.attr('y2', contentHeight1)
		;
	});
	mainAreaRect.on('mouseout', (e) => {
		tooltipElement.style('display', 'none');
		hoverLineElement.classed('show', false);
	});

	////////////////////////////////////////////////////////////////////////////////

	let lastSelection;

	const brush = d3.brushX()
		.extent([[0, 0], [contentWidth, contentHeight2]])
		.on("brush end", () => {
			if (d3.event.sourceEvent && d3.event.sourceEvent.type === 'zoom') return;
			const selection = lastSelection = d3.event.selection || context.xScale.time.range();
			main.xScale.time.domain(selection.map(context.xScale.time.invert, context.xScale.time));
			main.lineElem.rtcTemperature.attr("d", main.lineGen.rtcTemperature);
			main.lineElem.waterTemperature.attr("d", main.lineGen.waterTemperature);
			main.lineElem.phLevel.attr("d", main.lineGen.phLevel);
			main.xAxisElem.time.call(main.xAxisGen.time);
			mainAreaRect.call(zoom.transform, d3.zoomIdentity
				.scale(contentWidth / (selection[1] - selection[0]))
				.translate(-selection[0], 0))
			;
		})
	;

	const zoom = d3.zoom()
		.scaleExtent([1, Infinity])
		.translateExtent([[0, 0], [contentWidth, contentHeight1]])
		.extent([[0, 0], [contentWidth, contentHeight1]])
		.on("zoom", () => {
			if (d3.event.sourceEvent && d3.event.sourceEvent.type === 'brush') return;
			const transform = d3.event.transform;
			main.xScale.time.domain(transform.rescaleX(context.xScale.time).domain());
			main.lineElem.rtcTemperature.attr("d", main.lineGen.rtcTemperature);
			main.lineElem.waterTemperature.attr("d", main.lineGen.waterTemperature);
			main.lineElem.phLevel.attr("d", main.lineGen.phLevel);
			main.xAxisElem.time.call(main.xAxisGen.time);
			lastSelection = main.xScale.time.range().map(transform.invertX, transform);
			brushElement.call(brush.move, lastSelection);
		})
	;

	////////////////////////////////////////////////////////////////////////////////

	const xlSerialToJsDate = (xlSerial) => {
		// From https://stackoverflow.com/a/22352911/4880243
		return new Date(Math.round((((xlSerial - 25569) * 24 * 60) + new Date().getTimezoneOffset()) * 60 * 1000))
	};
	let updating = false;
	let samples = [];
	let lastRow = 1; // start at header
	const updateChart = async () => {
		if (updating) return;
		try {
			const feedURL = `https://content-sheets.googleapis.com/v4/spreadsheets/1OeXW_dhXnBcgqe8lflZT3rbdBoCy9qz3bURGQ95YH9o/values/RawData!A${lastRow + 1}:E?valueRenderOption=UNFORMATTED_VALUE&key=AIzaSyCcI8elBAOvt-clz54qC5_lGTa9NyXPyFQ`;
			const data = await fetch(feedURL)
				.then(async (response) => {
					if (!response.ok) {
						let error;
						try {
							error = JSON.parse(await response.text()).error;
						}
						catch (e) {
							throw `${response.code} Error while fetching samples`;
						}
						throw `${error.code} ${error.message}`;
					}
					return response.json();
				})
			;
			lastRow += data.values.length;

			if (!data.values || data.values.length == 0) {
				console.warn("No any new records, logging is disabled, controller is offline or something went wrong...");
				return;
			}
			data.values.forEach((sample) => {
				samples.push({
					time: xlSerialToJsDate(sample[0]),
					rtc: parseFloat(sample[2]),
					water: parseFloat(sample[3]),
					ph: parseFloat(sample[4]),
				});
			});
			const now = new Date();
			samples = samples.filter((sample) => {
				return (now - sample.time) < (1000 * 60 * 60 * 24 * 8);
			});

			main.xScale.time.domain(d3.extent(samples, (sample) => sample.time));
			main.yScale.temperature.domain([
				d3.min(samples, (sample) => Math.min(sample.rtc, sample.water)),
				d3.max(samples, (sample) => Math.max(sample.rtc, sample.water)),
			]).nice();
			main.yScale.phLevel.domain([
				d3.min(samples, (sample) => sample.ph),
				d3.max(samples, (sample) => sample.ph),
			]).nice();
			context.xScale.time.domain(main.xScale.time.domain());
			context.yScale.temperature.domain(main.yScale.temperature.domain());
			context.yScale.phLevel.domain(main.yScale.phLevel.domain());

			main.lineElem.rtcTemperature.datum(samples).attr("d", main.lineGen.rtcTemperature);
			main.lineElem.waterTemperature.datum(samples).attr("d", main.lineGen.waterTemperature);
			main.lineElem.phLevel.datum(samples).attr("d", main.lineGen.phLevel);
			main.xAxisElem.time.call(main.xAxisGen.time);
			main.yAxisElem.temperature.call(main.yAxisGen.temperature);
			main.yAxisElem.phLevel.call(main.yAxisGen.phLevel);

			main.gridElement.selectAll("*").remove();
			main.gridElement.call(
				d3.axisLeft(main.yScale.temperature)
					.tickSize(-contentWidth)
					.tickFormat("")
			);

			context.lineElem.rtcTemperature.datum(samples).attr("d", context.lineGen.rtcTemperature);
			context.lineElem.waterTemperature.datum(samples).attr("d", context.lineGen.waterTemperature);
			context.lineElem.phLevel.datum(samples).attr("d", context.lineGen.phLevel);
			context.xAxisElem.time.call(context.xAxisGen.time);

			if (lastSelection === undefined) {
				const grasp = [new Date(new Date().getTime() - 24 * 60 * 60 * 1000), new Date()];
				const store = context.xScale.time.range().map(context.xScale.time.invert);
				lastSelection = [Math.max(grasp[0], store[0]), Math.min(grasp[1], store[1])].map(context.xScale.time);
			}
			brushElement.call(brush).call(brush.move, lastSelection);
			mainAreaRect.call(zoom);
		}
		finally {
			updating = false;
		}
	};

	// First update
	await updateChart();

	// Register update on 'Space' key press
	let timeout;
	window.addEventListener('keypress', (e) => {
		if (e.key !== ' ') return;
		clearTimeout(timeout);
		timeout = setTimeout(updateChart, 500);
	});

	// Expose to global
	globalThis.updateChart = updateChart;

	globalThis.focusChartSelectionOn = (startDate, endDate) => {
		const store = context.xScale.time.range().map(context.xScale.time.invert);
		lastSelection = [Math.max(startDate, store[0]), Math.min(endDate, store[1])].map(context.xScale.time);
		brushElement.call(brush).call(brush.move, lastSelection);
		mainAreaRect.call(zoom);
	};
})();