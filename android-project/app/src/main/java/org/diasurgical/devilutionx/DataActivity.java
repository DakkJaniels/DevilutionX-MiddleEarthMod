package org.diasurgical.devilutionx;

import android.app.Activity;
import android.app.DownloadManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.util.Locale;

public class DataActivity extends Activity {
	private ExternalFilesManager fileManager;
	private DownloadReceiver mReceiver;
	private boolean isDownloadingSpawn = false;
	private boolean isDownloadingTranslation = false;
	private boolean isDownloadingFonts = false;
	private int pendingDownloads = 0;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_data);

		((TextView) findViewById(R.id.full_guide)).setMovementMethod(LinkMovementMethod.getInstance());
		((TextView) findViewById(R.id.online_guide)).setMovementMethod(LinkMovementMethod.getInstance());
	}

	protected void onResume() {
		super.onResume();

		fileManager = new ExternalFilesManager(this);

		startGame();
	}

	public void startGame(View view) {
		startGame();
	}

	private void startGame() {
		if (isMissingGameData()) {
			Toast toast = Toast.makeText(getApplicationContext(), getString(R.string.missing_game_data), Toast.LENGTH_SHORT);
			toast.show();
			return;
		}

		Intent intent = new Intent(this, DevilutionXSDLActivity.class);
		startActivity(intent);
		this.finish();
	}

	protected void onDestroy() {
		if (mReceiver != null)
			unregisterReceiver(mReceiver);

		super.onDestroy();
	}

	protected boolean pendingTranslationFile(String language) {
		String lang = Locale.getDefault().toString();
		if (!lang.startsWith(language)) {
			return false;
		}

		String translationFile = language + ".mpq";
		if (fileManager.hasFile(translationFile)) {
			isDownloadingTranslation = false;
			return false;
		}

		if (isDownloadingTranslation) {
			return true;
		}

		isDownloadingTranslation = true;
		sendDownloadRequest(
				"https://github.com/diasurgical/devilutionx-assets/releases/download/v2/" + language + ".mpq",
				language + ".mpq",
				"Translation Data"
		);

		return true;
	}

	/**
	 * Check if the game data is present
	 */
	private boolean isMissingGameData() {
		String lang = Locale.getDefault().toString();
		if (pendingTranslationFile("pl") || pendingTranslationFile("ru")) {
			return true;
		}

		if (lang.startsWith("ko") || lang.startsWith("zh") || lang.startsWith("ja")) {
			File fonts_mpq = fileManager.getFile("/fonts.mpq");
			if (!fonts_mpq.exists() || fonts_mpq.length() == 53991069 /* v2 */) {
				if (!isDownloadingFonts) {
					fonts_mpq.delete();
					isDownloadingFonts = true;
					sendDownloadRequest(
						"https://github.com/diasurgical/devilutionx-assets/releases/download/v3/fonts.mpq",
						"fonts.mpq",
						"Extra Game Fonts"
					);
				}
				return true;
			}
		}

		return !fileManager.hasFile("diabdat.mpq") &&
				!fileManager.hasFile("DIABDAT.MPQ") &&
				(!fileManager.hasFile("spawn.mpq") || isDownloadingSpawn);
	}

	/**
	 * Start downloading the shareware
	 */
	public void sendDownloadRequest(View view) {
		isDownloadingSpawn = true;
		sendDownloadRequest(
			"https://github.com/diasurgical/devilutionx-assets/releases/download/v2/spawn.mpq",
			"spawn.mpq",
			getString(R.string.shareware_data)
		);

		view.setEnabled(false);

		Toast toast = Toast.makeText(getApplicationContext(), getString(R.string.download_started), Toast.LENGTH_SHORT);
		toast.show();
	}

	public void sendDownloadRequest(String url, String fileName, String description) {
		DownloadManager.Request request = new DownloadManager.Request(Uri.parse(url))
				.setTitle(fileName)
				.setDescription(description)
				.setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE);

		File file = fileManager.getFile(fileName);
		Uri destination = Uri.fromFile(file);
		request.setDestinationUri(destination);

		if (mReceiver == null) {
			mReceiver = new DownloadReceiver();
			registerReceiver(mReceiver, new IntentFilter("android.intent.action.DOWNLOAD_COMPLETE"));
		}

		DownloadManager downloadManager = (DownloadManager)this.getSystemService(Context.DOWNLOAD_SERVICE);
		pendingDownloads++;
		downloadManager.enqueue(request);
	}

	/**
	 * Start game when download finishes
	 */
	private class DownloadReceiver extends BroadcastReceiver {
		@Override
		public void onReceive(@NonNull Context context, @NonNull Intent intent) {
			long receivedID = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1L);
			DownloadManager mgr = (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);

			DownloadManager.Query query = new DownloadManager.Query();
			query.setFilterById(receivedID);
			Cursor cur = mgr.query(query);
			int index = cur.getColumnIndex(DownloadManager.COLUMN_STATUS);
			if (cur.moveToFirst()) {
				if (cur.getInt(index) == DownloadManager.STATUS_SUCCESSFUL) {
					pendingDownloads--;
				}
				if (cur.getInt(index) == DownloadManager.STATUS_FAILED) {
					isDownloadingSpawn = false;
					isDownloadingFonts = false;
					isDownloadingTranslation = false;
				}
			}
			cur.close();

			if (pendingDownloads == 0) {
				isDownloadingSpawn = false;
				isDownloadingFonts = false;
				isDownloadingTranslation = false;
				startGame();
				findViewById(R.id.download_button).setEnabled(true);
			}
		}
	}
}
