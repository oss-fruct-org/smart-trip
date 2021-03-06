package org.fruct.oss.tsp;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import com.squareup.sqlbrite.BriteDatabase;
import com.squareup.sqlbrite.SqlBrite;

import net.danlew.android.joda.JodaTimeAndroid;

import org.fruct.oss.tsp.database.BriteDatabaseRepo;
import org.fruct.oss.tsp.database.DatabaseOpenHelper;
import org.fruct.oss.tsp.database.DatabaseRepo;
import org.fruct.oss.tsp.util.Pref;
import org.mapsforge.map.android.graphics.AndroidGraphicFactory;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.lang.reflect.Method;
import java.util.UUID;

public class App extends Application {
	private static final Logger log = LoggerFactory.getLogger(App.class);

	private static App instance;

	private SqlBrite sqlbrite;

	private DatabaseOpenHelper databaseOpenHelper;
	private BriteDatabase briteDatabase;
	private BriteDatabaseRepo briteDatabaseRepo;

	@Override
	public void onCreate() {
		super.onCreate();
		instance = this;

		log.info("App started");
		PreferenceManager.setDefaultValues(getContext(), R.xml.preferences, true);
		JodaTimeAndroid.init(this);

		AndroidGraphicFactory.createInstance(this);

		setupUserId();
		setupStetho();
		setupDatabase();
	}

	private void setupUserId() {
		SharedPreferences pref = PreferenceManager.getDefaultSharedPreferences(getContext());
		if (Pref.getUserId(pref) == null) {
			Pref.setUserId(pref, UUID.randomUUID().toString());
		}
	}

	@SuppressWarnings("TryWithIdenticalCatches")
	private void setupStetho() {
		if (BuildConfig.DEBUG) {
			try {
				Class<?> stethoClass = Class.forName("com.facebook.stetho.Stetho");
				Method initializeWithDefaults = stethoClass.getDeclaredMethod("initializeWithDefaults", Context.class);
				initializeWithDefaults.invoke(null, this);
			} catch (Exception e) {
				log.error("Can't init stetho", e);
			}
		}
	}

	public static Context getContext() {
		return instance.getApplicationContext();
	}

	public static App getInstance() {
		return instance;
	}

	public DatabaseRepo getDatabase() {
		return briteDatabaseRepo;
	}

	private void setupDatabase() {
		sqlbrite = SqlBrite.create();

		databaseOpenHelper = new DatabaseOpenHelper(getApplicationContext());
		databaseOpenHelper.getWritableDatabase();

		briteDatabase = sqlbrite.wrapDatabaseHelper(databaseOpenHelper);
		briteDatabaseRepo = new BriteDatabaseRepo(briteDatabase);
	}
}
