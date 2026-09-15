# trainer/task.py
"""
Trains an Isolation Forest anomaly detector on normal motor vibration data.
Runs INSIDE the Vertex AI training container — receives args from job.run().
"""

import argparse
import joblib
import pandas as pd
from sklearn.ensemble import IsolationForest
from google.cloud import storage
import sklearn, joblib as jl


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--data-path', type=str, required=True,
                         help='GCS path to training CSV, e.g. gs://bucket/data.csv')
    parser.add_argument('--model-dir', type=str, required=True,
                         help='GCS path to save the trained model, e.g. gs://bucket/models/v1')
    parser.add_argument('--contamination', type=float, default=0.02,
                         help='Expected proportion of outliers in the "normal" training data')
    parser.add_argument('--n-estimators', type=int, default=200,
                         help='Number of trees in the Isolation Forest')
    return parser.parse_args()


def load_data(data_path: str) -> pd.DataFrame:
    # pandas can read directly from GCS paths (gs://...) if gcsfs is installed
    df = pd.read_csv(data_path)
    return df


def train_model(df: pd.DataFrame, contamination: float, n_estimators: int) -> IsolationForest:
    feature_cols = ['RPM', 'Fric_X(g)', 'Fatg_X(mm/s)', 'Fric_Y(g)', 'Fatg_Y(mm/s)', 'Fric_Z(g)', 'Fatg_Z(mm/s)']
    X = df[feature_cols]

    model = IsolationForest(
        n_estimators=n_estimators,
        contamination=contamination,
        random_state=42,
    )
    model.fit(X)
    return model


def save_model_to_gcs(model, model_dir: str):
    # Save locally first (inside the container's filesystem)
    local_path = 'model.joblib'
    joblib.dump(model, local_path)

    # Parse gs://bucket-name/path/to/dir into bucket + blob path
    assert model_dir.startswith('gs://')
    path_parts = model_dir.replace('gs://', '').split('/', 1)
    bucket_name = path_parts[0]
    prefix = path_parts[1] if len(path_parts) > 1 else ''
    blob_path = f'{prefix}/model.joblib' if prefix else 'model.joblib'

    client = storage.Client()
    bucket = client.bucket(bucket_name)
    bucket.blob(blob_path).upload_from_filename(local_path)
    print(f'Model saved to {model_dir}/model.joblib')


def main():
    args = parse_args()

    print(f'Loading data from {args.data_path}')
    df = load_data(args.data_path)
    print(f'Loaded {len(df)} rows')

    print(f'Training Isolation Forest (contamination={args.contamination}, n_estimators={args.n_estimators})')
    model = train_model(df, args.contamination, args.n_estimators)

    print(f'Saving model to {args.model_dir}')
    save_model_to_gcs(model, args.model_dir)

    print('Training complete.')


if __name__ == '__main__':
    main()