import pytest
from app import app, db, Role, User, Card
from werkzeug.security import generate_password_hash

@pytest.fixture
def client():
    app.config['TESTING'] = True
    app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///:memory:'
    with app.test_client() as client:
        with app.app_context():
            db.create_all()
            
            # Roles
            r1 = Role(name='superadmin')
            r2 = Role(name='admin')
            r3 = Role(name='spectator')
            db.session.add_all([r1, r2, r3])
            db.session.commit()
            
            # Users
            u1 = User(username='superadmin', password_hash=generate_password_hash('pw'), role_id=r1.id)
            u2 = User(username='admin', password_hash=generate_password_hash('pw'), role_id=r2.id)
            u3 = User(username='spectator', password_hash=generate_password_hash('pw'), role_id=r3.id)
            db.session.add_all([u1, u2, u3])
            db.session.commit()
            
        yield client
        with app.app_context():
            db.drop_all()

def login(client, username, password):
    return client.post('/login', data=dict(
        username=username,
        password=password
    ), follow_redirects=True)

def test_spectator_cannot_access_users(client):
    login(client, 'spectator', 'pw')
    rv = client.get('/users')
    assert rv.status_code == 403

def test_admin_can_access_users(client):
    login(client, 'admin', 'pw')
    rv = client.get('/users')
    assert rv.status_code == 200

def test_verify_api_unchanged(client):
    rv = client.post('/verify', json={"number": "1234567890"})
    assert rv.status_code == 200
    assert rv.json['success'] == True
    assert rv.json['access'] == 'denied' # not in db
