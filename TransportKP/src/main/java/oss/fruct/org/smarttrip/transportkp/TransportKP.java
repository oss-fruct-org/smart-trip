package oss.fruct.org.smarttrip.transportkp;

import com.graphhopper.GraphHopper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import oss.fruct.org.smarttrip.transportkp.data.Point;
import oss.fruct.org.smarttrip.transportkp.data.RouteRequest;
import oss.fruct.org.smarttrip.transportkp.data.RouteResponse;
import oss.fruct.org.smarttrip.transportkp.smartspace.SmartSpace;
import oss.fruct.org.smarttrip.transportkp.tsp.ClosedStateTransition;
import oss.fruct.org.smarttrip.transportkp.tsp.GraphhopperGraphFactory;
import oss.fruct.org.smarttrip.transportkp.tsp.TravellingSalesman;

import java.util.Random;

public class TransportKP {
	private Logger log = LoggerFactory.getLogger(TransportKP.class);

	private SmartSpace smartSpace;
	private GraphHopper graphHopper;
	private Random random = new Random();

	public TransportKP(SmartSpace smartSpace, GraphHopper graphHopper) {
		this.smartSpace = smartSpace;
		this.graphHopper = graphHopper;
	}

	public void start() {
		if (!smartSpace.init()) {
			throw new RuntimeException("Can't connect to smartspace");
		}

		if (!smartSpace.subscribe()) {
			throw new RuntimeException("Can't subscribe to schedule request");
		}

		while(process())
			;

		smartSpace.unsubscribe();
		smartSpace.shutdown();
	}

	private boolean process() {
		log.info("Waiting subscription");
		RouteRequest request = smartSpace.waitSubscription();
		if (request == null) {
			return true;
		}

		log.info("Received request {}", request.getId());

		Point[] points = processRequest(request);

		// TODO: handle empty request
		for (Point point : points) {
			log.debug(point.getLat() + " " + point.getLon());
		}

		smartSpace.publish(new RouteResponse(points, "foot", request.getId()));
		return true;
	}

	private Point[] processRequest(RouteRequest request) {
		TravellingSalesman tsp = new TravellingSalesman(new GraphhopperGraphFactory(graphHopper),
				new ClosedStateTransition(random), request.getPoints(), random);

		return null;
		//return tsp.findPath();
	}
}
