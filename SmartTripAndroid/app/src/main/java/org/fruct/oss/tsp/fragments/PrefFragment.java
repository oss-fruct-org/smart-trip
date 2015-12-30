package org.fruct.oss.tsp.fragments;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.fruct.oss.tsp.R;
import org.fruct.oss.tsp.util.Pref;

public class PrefFragment extends PreferenceFragmentCompat implements SharedPreferences.OnSharedPreferenceChangeListener {
	private SharedPreferences pref;

	@Override
	public void onCreatePreferences(Bundle bundle, String rootKey) {
		addPreferencesFromResource(R.xml.preferences);
		pref = getPreferenceManager().getSharedPreferences();
	}

	@Override
	public void onResume() {
		super.onResume();
		pref.registerOnSharedPreferenceChangeListener(this);
		onSharedPreferenceChanged(pref, Pref.PREF_RADIUS);
	}

	@Override
	public void onPause() {
		pref.unregisterOnSharedPreferenceChangeListener(this);
		super.onPause();
	}

	private void setupRadiusPreference(SharedPreferences pref, int radius) {
		Preference radiusPreference = findPreference(Pref.PREF_RADIUS);
		radiusPreference.setSummary(getResources().getQuantityString(R.plurals.pref_radius_summary, radius, radius));
	}

	@Override
	public void onSharedPreferenceChanged(SharedPreferences pref, String key) {
		switch (key) {
		case Pref.PREF_RADIUS:
			setupRadiusPreference(pref, Pref.getRadius(pref));
			break;
		}
	}
}