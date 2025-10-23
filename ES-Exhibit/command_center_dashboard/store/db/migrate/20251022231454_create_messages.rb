class CreateMessages < ActiveRecord::Migration[8.0]
  def change
    create_table :messages do |t|
      t.string :sender_tag
      t.text :message
      t.string :nonce

      t.timestamps
    end
      add_index :messages, :nonce
      add_index :messages, :sender_tag
      add_index :messages, :created_at
  end
end
