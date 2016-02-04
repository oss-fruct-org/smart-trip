package oss.fruct.org.smarttrip.transportkp.data;

public class RouteResponse {
	private Point[] route;
	private double[] weights;

	private String roadType;
	private Object tag;

	public RouteResponse(Point[] route, double[] weight, String roadType, Object tag) {
		if (route.length != weight.length + 1) {
			throw new IllegalArgumentException(String.format("weights array must be equal points array - 1: %d != %d - 1",
					weight.length, route.length - 1));
		}

		this.route = route;
		this.roadType = roadType;
		this.tag = tag;
	}

	public Point[] getRoute() {
		return route;
	}

	public Object getRequestTag() {
		return tag;
	}

	public String getRoadType() {
		return roadType;
	}

	public double[] getWeights() {
		return weights;
	}
}
