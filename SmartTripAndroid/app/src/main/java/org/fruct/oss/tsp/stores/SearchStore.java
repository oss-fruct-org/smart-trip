package org.fruct.oss.tsp.stores;

import android.support.annotation.NonNull;

import org.fruct.oss.tsp.commondatatype.Movement;
import org.fruct.oss.tsp.commondatatype.Point;
import org.fruct.oss.tsp.events.GeoStoreChangedEvent;
import org.fruct.oss.tsp.events.SearchEvent;
import org.fruct.oss.tsp.events.SearchStoreChangedEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Collections;
import java.util.List;

import de.greenrobot.event.EventBus;
import rx.Observable;
import rx.subjects.BehaviorSubject;
import rx.subjects.PublishSubject;

/**
 * Локальное хранилище данных о загруженных географических объектах.
 *
 * Подписывается на событие обновления и обновляет свое состояние при получении события.
 */
public class SearchStore implements Store {
	private static final Logger log = LoggerFactory.getLogger(SearchStore.class);

	private BehaviorSubject<List<Point>> pointsSubject = BehaviorSubject.create();
	private PublishSubject<Object> updatedSubject = PublishSubject.create();

	@Override
	public void start() {
		EventBus.getDefault().register(this);
		clear();
	}

	@Override
	public void stop() {
		EventBus.getDefault().unregister(this);
	}

	public Observable<List<Point>> getObservable() {
		return pointsSubject;
	}

	public Observable<Object> getUpdatedObservable() {
		return updatedSubject;
	}

	public void onEvent(SearchEvent searchEvent) {
		log.debug("Search store updated");
		pointsSubject.onNext(searchEvent.getPoints());
		updatedSubject.onNext(null);
	}

	public void clear() {
		pointsSubject.onNext(Collections.<Point>emptyList());
	}

	public void removePoint(Point point) {

	}
}
