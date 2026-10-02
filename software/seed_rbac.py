import os
from app import app, db, Role, User
from werkzeug.security import generate_password_hash

def seed_rbac():
    with app.app_context():
        # Create roles
        roles_data = ['superadmin', 'admin', 'spectator']
        for r_name in roles_data:
            if not Role.query.filter_by(name=r_name).first():
                db.session.add(Role(name=r_name))
        db.session.commit()

        roles = {r.name: r for r in Role.query.all()}

        # Create users
        users_data = [
            {'username': 'superadmin', 'password': 'superadmin123', 'role': 'superadmin'},
            {'username': 'admin', 'password': 'admin123', 'role': 'admin'},
            {'username': 'spectator', 'password': 'spectator123', 'role': 'spectator'},
        ]

        for u_data in users_data:
            if not User.query.filter_by(username=u_data['username']).first():
                user = User(
                    username=u_data['username'],
                    password_hash=generate_password_hash(u_data['password']),
                    role_id=roles[u_data['role']].id,
                    is_active=True
                )
                db.session.add(user)
                print(f"Created user: {u_data['username']}")
        
        db.session.commit()
        print("RBAC seeded successfully.")

if __name__ == '__main__':
    seed_rbac()
